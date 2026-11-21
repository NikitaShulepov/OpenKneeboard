// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <VDXROVR/APIEvent.hpp>
#include <VDXROVR/RayIntersectsRect.hpp>
#include <VDXROVR/SHM/ActiveConsumers.hpp>
#include <VDXROVR/VRKneeboard.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <ranges>

using namespace DirectX::SimpleMath;

namespace VDXROVR {

namespace {
Matrix MakeHorizonAlignedAnchor(
  const VRKneeboard::Pose& hmdPose,
  const VRHeadFollowSettings& follow) {
  if (!follow.mLockRoll && !follow.mLockPitch) {
    return Matrix::CreateFromQuaternion(hmdPose.mOrientation)
      * Matrix::CreateTranslation(hmdPose.mPosition);
  }
  const auto forward = Vector3::Transform(Vector3::Forward, hmdPose.mOrientation);
  const float yaw = std::atan2(-forward.x, -forward.z);
  const float horiz = std::hypot(forward.x, forward.z);
  const float pitch = follow.mLockPitch
    ? 0.0f
    : std::atan2(forward.y, (std::max)(1e-5f, horiz));

  const auto q = Quaternion::CreateFromYawPitchRoll(yaw, pitch, 0.0f);
  return Matrix::CreateFromQuaternion(q)
    * Matrix::CreateTranslation(hmdPose.mPosition);
}

Matrix MakeHandOffsetAnchor(
  const VRKneeboard::Pose& handMountPose,
  const VRHeadFollowSettings&) {
  return Matrix::CreateFromQuaternion(handMountPose.mOrientation)
    * Matrix::CreateTranslation(handMountPose.mPosition);
}

Quaternion QuatFromTo(const Vector3& fromDir, const Vector3& toDir) {
  Vector3 a = fromDir;
  Vector3 b = toDir;
  if (a.LengthSquared() < 1e-10f || b.LengthSquared() < 1e-10f) {
    return Quaternion::Identity;
  }
  a.Normalize();
  b.Normalize();
  const float dot = std::clamp(a.Dot(b), -1.0f, 1.0f);
  if (dot > 0.99999f) {
    return Quaternion::Identity;
  }
  if (dot < -0.99999f) {
    Vector3 ortho = Vector3::UnitX.Cross(a);
    if (ortho.LengthSquared() < 1e-8f) {
      ortho = Vector3::UnitY.Cross(a);
    }
    ortho.Normalize();
    return Quaternion(ortho.x, ortho.y, ortho.z, 0.0f);
  }
  const Vector3 c = a.Cross(b);
  Quaternion q(c.x, c.y, c.z, 1.0f + dot);
  q.Normalize();
  return q;
}

Vector3 ComputeWristOrbitDragPointDir(
  const Vector3& rayOrigin,
  const Vector3& rayDir,
  const float grabDistance,
  const Vector3& wristPos,
  const Vector3& fallbackDir) {
  Vector3 d = rayDir;
  if (d.LengthSquared() < 1e-10f) {
    return fallbackDir;
  }
  d.Normalize();
  const Vector3 dragPoint =
    rayOrigin + (d * (std::max)(0.05f, grabDistance));
  Vector3 rel = dragPoint - wristPos;
  if (rel.LengthSquared() > 1e-8f) {
    rel.Normalize();
    return rel;
  }
  return fallbackDir;
}

Quaternion ComputeWristLocalOrientation(
  const Vector3& localPos,
  const Vector3& forwardHint,
  const float twistDeg) {
  Vector3 up = localPos;
  if (up.LengthSquared() >= 1e-8f) {
    up.Normalize();
  } else {
    up = Vector3::UnitY;
  }

  const auto projectPerp = [&](const Vector3& v) {
    return v - (up * v.Dot(up));
  };

  Vector3 forwardProj = projectPerp(forwardHint);
  if (forwardProj.LengthSquared() < 1e-8f) {
    forwardProj = projectPerp(Vector3::Backward);
  }
  if (forwardProj.LengthSquared() < 1e-8f) {
    forwardProj = projectPerp(Vector3::UnitX);
  }
  if (forwardProj.LengthSquared() < 1e-8f) {
    forwardProj = projectPerp(Vector3::UnitY);
  }
  if (forwardProj.LengthSquared() < 1e-10f) {
    return Quaternion::Identity;
  }
  forwardProj.Normalize();

  if (std::abs(twistDeg) > 1e-4f) {
    const float rad = DirectX::XMConvertToRadians(twistDeg);
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    const Vector3 uxF = up.Cross(forwardProj);
    forwardProj = (forwardProj * c) + (uxF * s);
    if (forwardProj.LengthSquared() > 1e-10f) {
      forwardProj.Normalize();
    }
  }

  Vector3 right = up.Cross(forwardProj);
  if (right.LengthSquared() < 1e-10f) {
    return Quaternion::Identity;
  }
  right.Normalize();

  Vector3 forwardZ = right.Cross(up);
  forwardZ.Normalize();
  const Matrix rotMat(
    right.x, right.y, right.z, 0.0f,
    up.x, up.y, up.z, 0.0f,
    forwardZ.x, forwardZ.y, forwardZ.z, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);
  return Quaternion::CreateFromRotationMatrix(rotMat);
}

Quaternion ComputeWristAxisOrientation(
  const Vector3& boardPos,
  const Vector3& wristPos,
  const Vector3& eyePos,
  const Quaternion& handOrient,
  const Vector3* fallbackForwardWorld = nullptr) {
  Vector3 up = boardPos - wristPos;
  if (up.LengthSquared() >= 1e-8f) {
    up.Normalize();
  } else {
    up = Vector3::Transform(Vector3::UnitY, handOrient);
    if (up.LengthSquared() >= 1e-10f) {
      up.Normalize();
    } else {
      up = Vector3::UnitY;
    }
  }

  const auto projectPerp = [&](const Vector3& v) {
    return v - (up * v.Dot(up));
  };

  Vector3 forwardProj = projectPerp(eyePos - boardPos);
  if (forwardProj.LengthSquared() < (0.05f * 0.05f)) {
    if (fallbackForwardWorld) {
      const Vector3 fb = projectPerp(*fallbackForwardWorld);
      if (fb.LengthSquared() >= 1e-8f) {
        forwardProj = fb;
      }
    }
  }
  if (forwardProj.LengthSquared() < 1e-8f) {
    forwardProj = projectPerp(Vector3::Transform(Vector3::Backward, handOrient));
  }
  if (forwardProj.LengthSquared() < 1e-8f) {
    forwardProj = projectPerp(Vector3::Backward);
  }
  if (forwardProj.LengthSquared() < 1e-8f) {
    forwardProj = projectPerp(Vector3::UnitX);
  }
  if (forwardProj.LengthSquared() < 1e-10f) {
    return Quaternion::Identity;
  }
  forwardProj.Normalize();

  Vector3 right = up.Cross(forwardProj);
  if (right.LengthSquared() < 1e-10f) {
    return Quaternion::Identity;
  }
  right.Normalize();

  Vector3 forwardZ = right.Cross(up);
  forwardZ.Normalize();
  const Matrix rotMat(
    right.x, right.y, right.z, 0.0f,
    up.x, up.y, up.z, 0.0f,
    forwardZ.x, forwardZ.y, forwardZ.z, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);
  return Quaternion::CreateFromRotationMatrix(rotMat);
}

float QuaternionAngleDeg(const Quaternion& a, const Quaternion& b) {
  const float dot = std::clamp(std::abs(a.Dot(b)), 0.0f, 1.0f);
  return DirectX::XMConvertToDegrees(2.0f * std::acos(dot));
}

bool RayPlaneIntersection(
  const VRKneeboard::Pose& controllerPose,
  const Vector3& planePoint,
  const Vector3& planeNormal,
  float& outDistance,
  Vector3& outPoint,
  const bool frontFaceOnly = true) {
  auto rayDir = Vector3::Transform(Vector3::Forward, controllerPose.mOrientation);
  if (rayDir.LengthSquared() < 1e-6f) {
    return false;
  }
  rayDir.Normalize();
  if (frontFaceOnly) {
    if (rayDir.Dot(planeNormal) >= -1e-4f) {
      return false;
    }
    if ((controllerPose.mPosition - planePoint).Dot(planeNormal) <= 1e-4f) {
      return false;
    }
  }
  const Ray ray(controllerPose.mPosition, rayDir);
  const Plane plane(planePoint, planeNormal);
  float dist = 0.0f;
  if (!ray.Intersects(plane, dist) || dist < 0.02f || dist > 6.0f) {
    return false;
  }
  outDistance = dist;
  outPoint = controllerPose.mPosition + (rayDir * dist);
  return true;
}

bool IsFrontFacingBoardAndRay(
  const VRKneeboard::Pose& rayOriginPose,
  const Vector3& boardPos,
  const Vector3& boardNormal) {
  auto rayDir = Vector3::Transform(Vector3::Forward, rayOriginPose.mOrientation);
  if (rayDir.LengthSquared() < 1e-6f) {
    return false;
  }
  rayDir.Normalize();
  return (rayOriginPose.mPosition - boardPos).Dot(boardNormal) > 1e-4f
    && rayDir.Dot(boardNormal) < -1e-4f;
}

std::pair<float, float> GetCornerSigns(VRKneeboard::EditCorner corner) {
  switch (corner) {
    case VRKneeboard::EditCorner::NW:
      return {-1.0f, 1.0f};
    case VRKneeboard::EditCorner::NE:
      return {1.0f, 1.0f};
    case VRKneeboard::EditCorner::SW:
      return {-1.0f, -1.0f};
    case VRKneeboard::EditCorner::SE:
      return {1.0f, -1.0f};
    default:
      return {0.0f, 0.0f};
  }
}

constexpr float kVRDockGapM = 0.018f;
constexpr float kVRDockHandleGapM = 0.008f;
constexpr float kVRDockAutoHideDelayS = 0.35f;
constexpr float kHandDistanceMinM = 0.06f;
constexpr float kHandDistanceMaxM = 0.65f;
constexpr float kHandDistanceStepM = 0.03f;
constexpr float kDepthStepM = 0.05f;

void ComputeVRDockHandleQuad(
  const VRKneeboard::Pose& boardPose,
  const Vector2& boardSize,
  VRKneeboard::Pose& outPose,
  Vector2& outSize) {
  const float handleWidth = std::clamp(boardSize.x * 0.24f, 0.056f, 0.088f);
  const float handleHeight =
    std::clamp(handleWidth * (32.0f / 128.0f), 0.015f, 0.024f);
  const Vector3 boardUp =
    Vector3::Transform(Vector3::UnitY, boardPose.mOrientation);
  const float offsetBelow =
    (boardSize.y * 0.5f) + kVRDockHandleGapM + (handleHeight * 0.5f);
  outPose = {
    .mPosition = boardPose.mPosition - (boardUp * offsetBelow),
    .mOrientation = boardPose.mOrientation,
  };
  outSize = {handleWidth, handleHeight};
}

bool IsGazeNearOverlayOrDock(
  const VRKneeboard::Pose& hmdPose,
  const VRKneeboard::Pose& boardPose,
  const Vector2& boardSize,
  const Vector2& dockSize) {
  const Vector3 normal =
    Vector3::Transform(Vector3::Backward, boardPose.mOrientation);
  const Vector3 gazeDir =
    Vector3::Transform(Vector3::Forward, hmdPose.mOrientation);
  if (
    gazeDir.Dot(normal) >= -0.15f
    || (hmdPose.mPosition - boardPose.mPosition).Dot(normal) <= 1e-4f) {
    return false;
  }
  const float extraBelow = kVRDockGapM + dockSize.y + 0.045f;
  const Vector2 combinedSize {
    (std::max)(boardSize.x, dockSize.x) * 1.12f,
    (boardSize.y * 1.06f) + extraBelow,
  };
  const Vector3 boardUp =
    Vector3::Transform(Vector3::UnitY, boardPose.mOrientation);
  const Vector3 combinedCenter =
    boardPose.mPosition - (boardUp * (extraBelow * 0.5f));
  return RayIntersectsRect(
    hmdPose.mPosition,
    hmdPose.mOrientation,
    combinedCenter,
    boardPose.mOrientation,
    combinedSize);
}

void ComputeVRDockQuad(
  const VRKneeboard::Pose& boardPose,
  const Vector2& boardSize,
  VRKneeboard::Pose& outPose,
  Vector2& outSize) {
  const float dockWidth = std::clamp(boardSize.x, 0.26f, 0.48f);
  const float dockHeight = dockWidth * (72.0f / 768.0f);
  const Vector3 boardUp =
    Vector3::Transform(Vector3::UnitY, boardPose.mOrientation);
  const float offsetBelow =
    (boardSize.y * 0.5f) + kVRDockGapM + (dockHeight * 0.5f);
  outPose = {
    .mPosition = boardPose.mPosition - (boardUp * offsetBelow),
    .mOrientation = boardPose.mOrientation,
  };
  outSize = {dockWidth, dockHeight};
}

VRKneeboard::VRDockButton HitTestVRDockUV(
  const float u,
  const float v,
  const bool canClose = true) {
  if (v < 0.04f || v > 0.96f || u < 0.0f || u > 1.0f) {
    return VRKneeboard::VRDockButton::None;
  }
  if (v < 0.24f && u >= 0.42f && u <= 0.58f) {
    return VRKneeboard::VRDockButton::Toggle;
  }
  if (u >= 0.012f && u <= 0.132f) {
    return VRKneeboard::VRDockButton::World;
  }
  if (u >= 0.140f && u <= 0.305f) {
    return VRKneeboard::VRDockButton::Head;
  }
  if (u >= 0.313f && u <= 0.448f) {
    return VRKneeboard::VRDockButton::LeftHand;
  }
  if (u >= 0.456f && u <= 0.591f) {
    return VRKneeboard::VRDockButton::RightHand;
  }
  if (u >= 0.606f && u <= 0.666f) {
    return VRKneeboard::VRDockButton::DistMinus;
  }
  if (u >= 0.752f && u <= 0.812f) {
    return VRKneeboard::VRDockButton::DistPlus;
  }
  if (u >= 0.824f && u <= (canClose ? 0.916f : 0.988f)) {
    return VRKneeboard::VRDockButton::SpawnTab;
  }
  if (canClose && u >= 0.924f && u <= 0.988f) {
    return VRKneeboard::VRDockButton::CloseOverlay;
  }
  return VRKneeboard::VRDockButton::None;
}

bool IsHeadFollowMode(const VRFollowMode mode) noexcept {
  return mode == VRFollowMode::HeadFollowLazy
    || mode == VRFollowMode::HeadFollowSmooth
    || mode == VRFollowMode::HeadLocked;
}

VRFollowMode CycleHeadFollowMode(const VRFollowMode current) noexcept {
  switch (current) {
    case VRFollowMode::HeadFollowLazy:
      return VRFollowMode::HeadFollowSmooth;
    case VRFollowMode::HeadFollowSmooth:
      return VRFollowMode::HeadLocked;
    case VRFollowMode::HeadLocked:
    default:
      return VRFollowMode::HeadFollowLazy;
  }
}

bool IsHandPinnedMode(const VRFollowMode mode) noexcept {
  return mode == VRFollowMode::LeftHand || mode == VRFollowMode::RightHand;
}

VRKneeboard::Pose ApplyHandBottomEdgeAnchor(
  const VRKneeboard::Pose& bottomAnchorPose,
  const float overlayHeightM) noexcept {
  if (!(overlayHeightM > 1e-4f)) {
    return bottomAnchorPose;
  }
  const Vector3 boardUp =
    Vector3::Transform(Vector3::UnitY, bottomAnchorPose.mOrientation);
  return {
    .mPosition = bottomAnchorPose.mPosition + (boardUp * (overlayHeightM * 0.5f)),
    .mOrientation = bottomAnchorPose.mOrientation,
  };
}

Vector3 ExtractHandBottomEdgePosition(
  const VRKneeboard::Pose& centerPose,
  const float overlayHeightM) noexcept {
  if (!(overlayHeightM > 1e-4f)) {
    return centerPose.mPosition;
  }
  const Vector3 boardUp =
    Vector3::Transform(Vector3::UnitY, centerPose.mOrientation);
  return centerPose.mPosition - (boardUp * (overlayHeightM * 0.5f));
}

float GetHandDistanceMeters(const VRHeadFollowSettings& hf) noexcept {
  const float len =
    std::hypot(hf.mHandOffsetX, std::hypot(hf.mHandOffsetY, hf.mHandOffsetZ));
  return std::clamp(len > 1e-4f ? len : 0.10f, kHandDistanceMinM, kHandDistanceMaxM);
}

void SetHandDistanceMeters(VRHeadFollowSettings& hf, const float targetDistM) noexcept {
  const Vector3 v(hf.mHandOffsetX, hf.mHandOffsetY, hf.mHandOffsetZ);
  const float curLen = v.Length();
  const Vector3 dir = (curLen > 1e-4f) ? (v / curLen) : Vector3::UnitY;
  const float clampedR = std::clamp(targetDistM, kHandDistanceMinM, kHandDistanceMaxM);
  hf.mHandOffsetX = std::clamp(dir.x * clampedR, -kHandDistanceMaxM, kHandDistanceMaxM);
  hf.mHandOffsetY = std::clamp(dir.y * clampedR, -0.45f, kHandDistanceMaxM);
  hf.mHandOffsetZ = std::clamp(dir.z * clampedR, -kHandDistanceMaxM, kHandDistanceMaxM);
}

float ComputeWristRadialDragRadius(
  const Vector3& rayOrigin,
  const Vector3& rayDir,
  const float grabDistance,
  const Vector3& wristPos,
  const float fallbackRadius) {
  Vector3 d = rayDir;
  if (d.LengthSquared() < 1e-10f) {
    return std::clamp(fallbackRadius, kHandDistanceMinM, kHandDistanceMaxM);
  }
  d.Normalize();
  const Vector3 dragPoint = rayOrigin + (d * (std::max)(0.05f, grabDistance));
  const float radialLen = (dragPoint - wristPos).Length();
  if (!std::isfinite(radialLen) || radialLen < 1e-4f) {
    return std::clamp(fallbackRadius, kHandDistanceMinM, kHandDistanceMaxM);
  }
  return std::clamp(radialLen, kHandDistanceMinM, kHandDistanceMaxM);
}

}// namespace

VRKneeboard::ManipulationState::LayerVRState&
VRKneeboard::GetOrCreateLayerState(
  const uint64_t layerID,
  const bool isPrimary,
  const VRHeadFollowSettings& shmFollow) noexcept {
  auto [it, inserted] = mManipulation.mLayerStates.try_emplace(layerID);
  auto& st = it->second;
  if (inserted) {
    st.mIsPrimary = isPrimary;
    st.mLastSeenSHMHeadFollow = shmFollow;
    if (isPrimary) {
      st.mOptimisticHeadFollow = shmFollow;
      st.mHasOptimisticVR = false;
    } else {
      st.mOptimisticHeadFollow = shmFollow;
      st.mOptimisticHeadFollow.mPinToHand = false;
      st.mOptimisticHeadFollow.mMode = VRFollowMode::WorldLocked;
      st.mHasOptimisticVR = true;
    }
    const auto eff = GetEffectiveFollowMode(st.mOptimisticHeadFollow);
    if (!IsHandPinnedMode(eff)) {
      st.mLastDetachedMode = eff;
      if (IsHeadFollowMode(eff)) {
        st.mLastHeadFollowMode = eff;
      }
    }
  } else {
    st.mIsPrimary = isPrimary;
  }
  return st;
}

const VRKneeboard::ManipulationState::LayerVRState*
VRKneeboard::FindLayerState(const uint64_t layerID) const noexcept {
  const auto it = mManipulation.mLayerStates.find(layerID);
  if (it == mManipulation.mLayerStates.end()) {
    return nullptr;
  }
  return &it->second;
}

VRRenderSettings VRKneeboard::GetEffectiveVRSettings(
  const VRRenderSettings& shmVR) const noexcept {
  if (mManipulation.mPrimaryLayerID != 0) {
    return this->GetEffectiveLayerVRSettings(
      shmVR, mManipulation.mPrimaryLayerID, true);
  }
  return shmVR;
}

VRRenderSettings VRKneeboard::GetEffectiveLayerVRSettings(
  const VRRenderSettings& shmVR,
  const uint64_t layerID,
  const bool isPrimaryLayer) const noexcept {
  auto ret = shmVR;
  const auto* st = this->FindLayerState(layerID);
  if (!isPrimaryLayer) {
    if (st && st->mHasOptimisticVR) {
      ret.mHeadFollow = st->mOptimisticHeadFollow;
    } else {
      ret.mHeadFollow.mPinToHand = false;
      ret.mHeadFollow.mMode = VRFollowMode::WorldLocked;
    }
    return ret;
  }
  if (!st || !st->mHasOptimisticVR) {
    return ret;
  }
  const auto now = std::chrono::steady_clock::now();
  const bool dragging =
    mManipulation.mDragMode != ManipulationState::DragMode::None
    && mManipulation.mTargetLayerID == layerID;
  const bool recent =
    (now - st->mLastChangeTime) < std::chrono::milliseconds(450);
  if (!dragging && !recent) {
    return ret;
  }
  if (!dragging && !(shmVR.mHeadFollow == st->mLastSeenSHMHeadFollow)) {
    return ret;
  }
  ret.mHeadFollow = st->mOptimisticHeadFollow;
  return ret;
}

void VRKneeboard::SolvePoseIntoSettings(
  VRHeadFollowSettings& hf,
  VRPose& worldPose,
  const Pose& targetPose,
  const Pose& hmdPose,
  const std::optional<Pose>& handMountPose,
  const float overlayHeightM) const {
  const auto effectiveMode = GetEffectiveFollowMode(hf);
  const auto targetMatrix =
    Matrix::CreateFromQuaternion(targetPose.mOrientation)
    * Matrix::CreateTranslation(targetPose.mPosition);

  if (effectiveMode == VRFollowMode::WorldLocked) {
    Matrix invRecenter = mRecenter.Invert();
    const auto localMatrix = targetMatrix * invRecenter;
    const float eyeHeight = mEyeHeight.value_or(hmdPose.mPosition.y);
    worldPose.mX = std::clamp(localMatrix._41, -1.6f, 1.6f);
    worldPose.mEyeY = std::clamp(localMatrix._42 - eyeHeight, -1.2f, 0.9f);
    worldPose.mZ = std::clamp(localMatrix._43, -1.6f, 0.8f);
    worldPose.mRY = std::asin(std::clamp(-localMatrix._13, -1.0f, 1.0f));
    if (std::abs(std::cos(worldPose.mRY)) < 1e-4f) {
      worldPose.mRX = 0.0f;
      worldPose.mRZ = std::atan2(-localMatrix._21, localMatrix._22);
    } else {
      worldPose.mRX = std::atan2(localMatrix._23, localMatrix._33);
      worldPose.mRZ = std::atan2(localMatrix._12, localMatrix._11);
    }
    return;
  }

  if (IsHandPinnedMode(effectiveMode) && handMountPose) {
    const Vector3 bottomWorld =
      ExtractHandBottomEdgePosition(targetPose, overlayHeightM);
    const auto bottomMatrix =
      Matrix::CreateFromQuaternion(targetPose.mOrientation)
      * Matrix::CreateTranslation(bottomWorld);
    const auto anchor = MakeHandOffsetAnchor(*handMountPose, hf);
    const auto handLocal = bottomMatrix * anchor.Invert();
    hf.mHandOffsetX = std::clamp(handLocal._41, -0.65f, 0.65f);
    hf.mHandOffsetY = std::clamp(handLocal._42, -0.45f, 0.65f);
    hf.mHandOffsetZ = std::clamp(handLocal._43, -0.65f, 0.65f);
    Vector3 localUp(hf.mHandOffsetX, hf.mHandOffsetY, hf.mHandOffsetZ);
    if (localUp.LengthSquared() >= 1e-8f) {
      localUp.Normalize();
    } else {
      localUp = Vector3::UnitY;
    }
    const auto unTilt = (std::abs(hf.mWristTiltOffset) > 1e-4f)
      ? Matrix::CreateRotationX(DirectX::XMConvertToRadians(-hf.mWristTiltOffset)) * handLocal
      : handLocal;
    const Vector3 localForwardRaw(unTilt._31, unTilt._32, unTilt._33);
    Vector3 perp = localForwardRaw - (localUp * localForwardRaw.Dot(localUp));
    if (perp.LengthSquared() >= 1e-8f) {
      perp.Normalize();
    } else {
      perp = Vector3::Backward;
    }
    hf.mHandForwardX = perp.x;
    hf.mHandForwardY = perp.y;
    hf.mHandForwardZ = perp.z;
    hf.mHandPitchDeg = 0.0f;
    return;
  }

  const auto anchor = MakeHorizonAlignedAnchor(hmdPose, hf);
  const auto headLocal = targetMatrix * anchor.Invert();
  hf.mHeadOffsetX = std::clamp(headLocal._41, -0.9f, 0.9f);
  hf.mHeadOffsetY = std::clamp(headLocal._42, -0.65f, 0.65f);
  hf.mHeadOffsetZ = std::clamp(headLocal._43, -1.35f, -0.20f);
  const float horiz = std::hypot(headLocal._31, headLocal._33);
  hf.mHeadPitchDeg = std::clamp(
    DirectX::XMConvertToDegrees(std::atan2(-headLocal._32, (std::max)(1e-5f, horiz))),
    -65.0f,
    65.0f);
  hf.mHeadYawDeg = std::clamp(
    DirectX::XMConvertToDegrees(std::atan2(headLocal._31, headLocal._33)),
    -80.0f,
    80.0f);
}

void VRKneeboard::SendManipulatedTransformEvent(
  const uint64_t layerID,
  const bool isPrimaryLayer,
  const bool force) {
  const auto now = std::chrono::steady_clock::now();
  if (
    !force
    && (now - mManipulation.mLastPersistTime) < std::chrono::milliseconds(130)) {
    mManipulation.mDirtyPendingPersist = true;
    return;
  }

  const auto* st = this->FindLayerState(layerID);
  if (!st) {
    return;
  }

  mManipulation.mLastPersistTime = now;
  mManipulation.mDirtyPendingPersist = false;

  SetVRManipulatedTransformEvent ev {};
  ev.mKneeboard = mManipulation.mTargetKneeboardIndex;
  ev.mLayerID = layerID;
  if (st->mHasOptimisticPose) {
    ev.mX = st->mOptimisticWorldPose.mX;
    ev.mEyeY = st->mOptimisticWorldPose.mEyeY;
    ev.mZ = st->mOptimisticWorldPose.mZ;
    ev.mRX = st->mOptimisticWorldPose.mRX;
    ev.mRY = st->mOptimisticWorldPose.mRY;
    ev.mRZ = st->mOptimisticWorldPose.mRZ;
  }
  if (isPrimaryLayer && st->mHasOptimisticVR) {
    const auto& hf = st->mOptimisticHeadFollow;
    ev.mHeadOffsetX = hf.mHeadOffsetX;
    ev.mHeadOffsetY = hf.mHeadOffsetY;
    ev.mHeadOffsetZ = hf.mHeadOffsetZ;
    ev.mHeadPitchDeg = hf.mHeadPitchDeg;
    ev.mHeadYawDeg = hf.mHeadYawDeg;
    ev.mHandOffsetX = hf.mHandOffsetX;
    ev.mHandOffsetY = hf.mHandOffsetY;
    ev.mHandOffsetZ = hf.mHandOffsetZ;
    ev.mHandPitchDeg = hf.mHandPitchDeg;
    ev.mHandForwardX = hf.mHandForwardX;
    ev.mHandForwardY = hf.mHandForwardY;
    ev.mHandForwardZ = hf.mHandForwardZ;
    ev.mFollowMode = static_cast<uint8_t>(hf.mMode);
    ev.mPinToHand = hf.mPinToHand;
    ev.mPinnedHand = hf.mPinnedHand;
  }
  if (st->mHasOptimisticSize) {
    ev.mMaxWidth = st->mOptimisticSize.x;
    ev.mMaxHeight = st->mOptimisticSize.y;
  }
  APIEvent::FromStruct(ev).Send();
}

void VRKneeboard::UpdateControllerManipulation(
  const SHM::Config& config,
  const std::span<const SHM::LayerConfig>& layers,
  const Pose& hmdPose,
  const std::optional<Pose>& handMountPose,
  const ControllerInput& controller,
  const float dtSeconds) {
  const std::array<ControllerInput, 1> single {controller};
  this->UpdateControllerManipulation(
    config, layers, hmdPose, handMountPose, single, dtSeconds);
}

void VRKneeboard::HandleVRDockButtonClick(
  const VRDockButton button,
  const bool canClose,
  const Pose& boardPose,
  const float boardHeightM,
  const Pose& hmdPose,
  const std::array<std::optional<Pose>, 2>& handMountPoses,
  const SHM::LayerConfig& workingLayer,
  const uint8_t targetIndex,
  const bool isPrimaryLayer,
  const SHM::Config& config) {
  if (button == VRDockButton::CloseOverlay && !canClose) {
    return;
  }

  auto& lState = this->GetOrCreateLayerState(
    workingLayer.mLayerID, isPrimaryLayer, config.mVR.mHeadFollow);
  if (isPrimaryLayer && !lState.mHasOptimisticVR) {
    const auto eff =
      this->GetEffectiveLayerVRSettings(config.mVR, workingLayer.mLayerID, true);
    lState.mOptimisticHeadFollow = eff.mHeadFollow;
  }
  lState.mLastSeenSHMHeadFollow = config.mVR.mHeadFollow;
  if (!lState.mHasOptimisticPose) {
    lState.mOptimisticWorldPose = workingLayer.mVR.mPose;
  }
  mManipulation.mTargetLayerID = workingLayer.mLayerID;
  mManipulation.mTargetKneeboardIndex = targetIndex;

  auto& hf = lState.mOptimisticHeadFollow;
  const auto currentEffective = GetEffectiveFollowMode(hf);
  const auto activeHandPose =
    (currentEffective == VRFollowMode::LeftHand)
      ? handMountPoses[0]
      : ((currentEffective == VRFollowMode::RightHand) ? handMountPoses[1]
                                                       : std::nullopt);

  if (button == VRDockButton::SpawnTab || button == VRDockButton::CloseOverlay) {
    VROverlayManageEvent ev {};
    ev.mAction = (button == VRDockButton::SpawnTab)
      ? VROverlayManageEvent::Action::SpawnNextTab
      : VROverlayManageEvent::Action::CloseOverlay;
    ev.mKneeboard = targetIndex;
    ev.mLayerID = workingLayer.mLayerID;
    if (button == VRDockButton::SpawnTab) {
      const Vector3 boardRight =
        Vector3::Transform(Vector3::UnitX, boardPose.mOrientation);
      Pose spawnPose = boardPose;
      spawnPose.mPosition = boardPose.mPosition + (boardRight * 0.34f);
      VRHeadFollowSettings worldHf = hf;
      worldHf.mPinToHand = false;
      worldHf.mMode = VRFollowMode::WorldLocked;
      VRPose solvedWorld = workingLayer.mVR.mPose;
      this->SolvePoseIntoSettings(
        worldHf, solvedWorld, spawnPose, hmdPose, std::nullopt, boardHeightM);
      ev.mSpawnX = solvedWorld.mX;
      ev.mSpawnEyeY = solvedWorld.mEyeY;
      ev.mSpawnZ = solvedWorld.mZ;
      ev.mSpawnRX = solvedWorld.mRX;
      ev.mSpawnRY = solvedWorld.mRY;
      ev.mSpawnRZ = solvedWorld.mRZ;
    }
    APIEvent::FromStruct(ev).Send();
    return;
  }

  if (button == VRDockButton::Toggle) {
    const bool nextExpanded = !lState.mDockExpanded;
    if (nextExpanded) {
      for (auto& [id, st] : mManipulation.mLayerStates) {
        if (id != workingLayer.mLayerID) {
          st.mDockExpanded = false;
          st.mDockLookAwaySeconds = 0.0f;
        }
      }
    }
    lState.mDockExpanded = nextExpanded;
    lState.mDockLookAwaySeconds = 0.0f;
    return;
  }

  if (button == VRDockButton::DistMinus || button == VRDockButton::DistPlus) {
    const float sign = (button == VRDockButton::DistPlus) ? 1.0f : -1.0f;
    if (
      currentEffective == VRFollowMode::LeftHand
      || currentEffective == VRFollowMode::RightHand) {
      SetHandDistanceMeters(
        hf, GetHandDistanceMeters(hf) + (sign * kHandDistanceStepM));
    } else if (currentEffective == VRFollowMode::WorldLocked) {
      Vector3 radial = boardPose.mPosition - hmdPose.mPosition;
      const float len = radial.Length();
      if (len > 0.05f) {
        radial /= len;
        const float nextLen = std::clamp(len + (sign * kDepthStepM), 0.22f, 1.45f);
        Pose targetPose = boardPose;
        targetPose.mPosition = hmdPose.mPosition + (radial * nextLen);
        this->SolvePoseIntoSettings(
          hf,
          lState.mOptimisticWorldPose,
          targetPose,
          hmdPose,
          activeHandPose);
      }
    } else {
      hf.mHeadOffsetZ =
        std::clamp(hf.mHeadOffsetZ - (sign * kDepthStepM), -1.35f, -0.20f);
    }
    lState.mHasOptimisticVR = true;
    lState.mHasOptimisticPose = true;
    lState.mLastChangeTime = std::chrono::steady_clock::now();
    this->SendManipulatedTransformEvent(
      workingLayer.mLayerID, isPrimaryLayer, true);
    return;
  }

  if (
    currentEffective != VRFollowMode::LeftHand
    && currentEffective != VRFollowMode::RightHand) {
    lState.mLastDetachedMode = currentEffective;
    if (IsHeadFollowMode(currentEffective)) {
      lState.mLastHeadFollowMode = currentEffective;
    }
  }

  if (button == VRDockButton::World) {
    hf.mPinToHand = false;
    hf.mMode = VRFollowMode::WorldLocked;
    lState.mLastDetachedMode = VRFollowMode::WorldLocked;
  } else if (button == VRDockButton::Head) {
    hf.mPinToHand = false;
    const VRFollowMode nextHead = IsHeadFollowMode(currentEffective)
      ? CycleHeadFollowMode(currentEffective)
      : (IsHeadFollowMode(lState.mLastHeadFollowMode)
           ? lState.mLastHeadFollowMode
           : VRFollowMode::HeadFollowLazy);
    hf.mMode = nextHead;
    lState.mLastHeadFollowMode = nextHead;
    lState.mLastDetachedMode = nextHead;
  } else if (
    button == VRDockButton::LeftHand || button == VRDockButton::RightHand) {
    const uint8_t requestedHand = (button == VRDockButton::RightHand) ? 1u : 0u;
    const VRFollowMode requestedMode =
      (requestedHand == 1u) ? VRFollowMode::RightHand : VRFollowMode::LeftHand;
    if (currentEffective == requestedMode) {
      hf.mPinToHand = false;
      hf.mMode = lState.mLastDetachedMode;
    } else {
      hf.mPinToHand = true;
      hf.mPinnedHand = requestedHand;
      if (
        hf.mMode == VRFollowMode::LeftHand
        || hf.mMode == VRFollowMode::RightHand) {
        hf.mMode = lState.mLastDetachedMode;
      }
    }
  }

  const auto nextEffective = GetEffectiveFollowMode(hf);
  const auto targetHandPose =
    (nextEffective == VRFollowMode::LeftHand)
      ? handMountPoses[0]
      : ((nextEffective == VRFollowMode::RightHand) ? handMountPoses[1]
                                                    : std::nullopt);

  if (IsHandPinnedMode(nextEffective)) {
    if (targetHandPose) {
      Quaternion invHandRot {};
      targetHandPose->mOrientation.Inverse(invHandRot);
      const Vector3 bottomWorld =
        ExtractHandBottomEdgePosition(boardPose, boardHeightM);
      const Vector3 localRel = Vector3::Transform(
        bottomWorld - targetHandPose->mPosition, invHandRot);
      const float rawLen = localRel.Length();
      const Vector3 localDir =
        (rawLen > 1e-4f) ? (localRel / rawLen) : Vector3::UnitY;
      const bool alreadyErgonomicAboveWrist =
        rawLen >= kHandDistanceMinM && rawLen <= 0.36f && localDir.y >= 0.70f;
      if (alreadyErgonomicAboveWrist) {
        this->SolvePoseIntoSettings(
          hf,
          lState.mOptimisticWorldPose,
          boardPose,
          hmdPose,
          targetHandPose,
          boardHeightM);
      } else {
        const float distM = GetHandDistanceMeters(hf);
        const Vector3 worldMountCenter = targetHandPose->mPosition
          + Vector3::Transform(
              Vector3(0.0f, distM + (boardHeightM * 0.5f), 0.0f),
              targetHandPose->mOrientation);
        const Vector3 localToHmd =
          Vector3::Transform(hmdPose.mPosition - worldMountCenter, invHandRot);
        Vector3 flatFwd(localToHmd.x, 0.0f, localToHmd.z);
        if (flatFwd.LengthSquared() >= 1e-8f) {
          flatFwd.Normalize();
        } else {
          flatFwd = Vector3::UnitZ;
        }
        hf.mHandOffsetX = 0.0f;
        hf.mHandOffsetY = distM;
        hf.mHandOffsetZ = 0.0f;
        hf.mHandForwardX = flatFwd.x;
        hf.mHandForwardY = 0.0f;
        hf.mHandForwardZ = flatFwd.z;
        hf.mHandPitchDeg = 0.0f;
      }
    }
  } else {
    this->SolvePoseIntoSettings(
      hf,
      lState.mOptimisticWorldPose,
      boardPose,
      hmdPose,
      std::nullopt);
  }

  auto& fState = mFollowStates[workingLayer.mLayerID];
  fState.mInitialized = true;
  fState.mEngaged = false;
  fState.mAnchorHmdPosition = hmdPose.mPosition;
  fState.mCurrentPose = IsHandPinnedMode(nextEffective)
    ? Pose {
        .mPosition = ExtractHandBottomEdgePosition(boardPose, boardHeightM),
        .mOrientation = boardPose.mOrientation,
      }
    : boardPose;
  fState.mLastEffectiveMode = nextEffective;

  lState.mHasOptimisticVR = true;
  lState.mHasOptimisticPose = true;
  lState.mLastChangeTime = std::chrono::steady_clock::now();
  this->SendManipulatedTransformEvent(
    workingLayer.mLayerID, isPrimaryLayer, true);
}

void VRKneeboard::UpdateControllerManipulation(
  const SHM::Config& config,
  const std::span<const SHM::LayerConfig>& layers,
  const Pose& hmdPose,
  const std::optional<Pose>& handMountPose,
  const std::span<const ControllerInput> controllers,
  const float dtSeconds) {
  const std::array<std::optional<Pose>, 2> handPoses {
    handMountPose, handMountPose};
  this->UpdateControllerManipulation(
    config, layers, hmdPose, handPoses, controllers, dtSeconds);
}

void VRKneeboard::UpdateControllerManipulation(
  const SHM::Config& config,
  const std::span<const SHM::LayerConfig>& layers,
  const Pose& hmdPose,
  const std::array<std::optional<Pose>, 2>& handMountPoses,
  const std::span<const ControllerInput> controllers,
  const float dtSeconds) {
  if (!mEyeHeight) {
    mEyeHeight = {hmdPose.mPosition.y};
  }
  mManipulation.mVisuals = {};

  struct LayerSnapshot {
    const SHM::LayerConfig* mSourceLayer {nullptr};
    SHM::LayerConfig mWorkingLayer {};
    VRRenderSettings mEffectiveVR {};
    Pose mBoardBasePose {};
    Pose mBoardPose {};
    Vector2 mBoardSize {};
    bool mIsPrimary {false};
    bool mIsLooking {false};
    uint8_t mKneeboardIndex {1};
  };

  std::vector<LayerSnapshot> visibleLayers;
  visibleLayers.reserve(layers.size());
  mManipulation.mPrimaryLayerID = 0;

  const auto now = std::chrono::steady_clock::now();
  std::vector<uint64_t> activeIds;
  activeIds.reserve(layers.size());

  for (size_t li = 0; li < layers.size(); ++li) {
    const auto& layer = layers[li];
    if (!layer.mVREnabled) {
      continue;
    }
    activeIds.push_back(layer.mLayerID);
    const bool isPrimary = visibleLayers.empty();
    if (isPrimary) {
      mManipulation.mPrimaryLayerID = layer.mLayerID;
    }
    auto& lState = this->GetOrCreateLayerState(
      layer.mLayerID, isPrimary, config.mVR.mHeadFollow);
    const bool draggingThis =
      mManipulation.mDragMode != ManipulationState::DragMode::None
      && mManipulation.mTargetLayerID == layer.mLayerID;
    const bool recentOpt =
      (now - lState.mLastChangeTime) < std::chrono::milliseconds(450);
    if (!draggingThis && !recentOpt) {
      if (isPrimary) {
        lState.mHasOptimisticVR = false;
      }
      lState.mHasOptimisticPose = false;
      lState.mHasOptimisticSize = false;
    }

    SHM::LayerConfig working = layer;
    if ((draggingThis || recentOpt) && lState.mHasOptimisticPose) {
      working.mVR.mPose = lState.mOptimisticWorldPose;
    }
    if (
      (draggingThis || recentOpt)
      && lState.mHasOptimisticSize
      && lState.mOptimisticSize.x > 0.05f
      && lState.mOptimisticSize.y > 0.05f) {
      working.mVR.mPhysicalSize = {
        lState.mOptimisticSize.x,
        lState.mOptimisticSize.y,
      };
    }

    const auto effVR = this->GetEffectiveLayerVRSettings(
      config.mVR, layer.mLayerID, isPrimary);
    const auto effMode = GetEffectiveFollowMode(effVR.mHeadFollow);
    const std::optional<Pose> lHandPose =
      (effMode == VRFollowMode::LeftHand)
        ? handMountPoses[0]
        : ((effMode == VRFollowMode::RightHand) ? handMountPoses[1]
                                                : std::nullopt);
    SHM::Config lCfg = config;
    lCfg.mVR = effVR;
    const Pose basePose =
      this->GetKneeboardPose(effVR, working, hmdPose, 0.0f, lHandPose);
    const bool looking =
      this->IsLookingAtKneeboard(lCfg, working, hmdPose, basePose);
    const Vector2 boardSz = this->GetKneeboardSize(lCfg, working, looking);
    const Pose boardPose = IsHandPinnedMode(effMode)
      ? ApplyHandBottomEdgeAnchor(basePose, boardSz.y)
      : basePose;

    visibleLayers.push_back({
      .mSourceLayer = &layer,
      .mWorkingLayer = working,
      .mEffectiveVR = effVR,
      .mBoardBasePose = basePose,
      .mBoardPose = boardPose,
      .mBoardSize = boardSz,
      .mIsPrimary = isPrimary,
      .mIsLooking = looking,
      .mKneeboardIndex = static_cast<uint8_t>(li + 1),
    });
  }

  for (auto it = mManipulation.mLayerStates.begin();
       it != mManipulation.mLayerStates.end();) {
    if (
      std::ranges::find(activeIds, it->first)
      == activeIds.end()) {
      it = mManipulation.mLayerStates.erase(it);
    } else {
      ++it;
    }
  }

  const auto primaryEffVR = this->GetEffectiveVRSettings(config.mVR);
  bool anyGripHeld = false;
  for (const auto& c : controllers) {
    if (c.mValid && c.mGripHeld) {
      anyGripHeld = true;
      break;
    }
  }
  const bool editActive = primaryEffVR.mAllowControllerManipulation
    && (primaryEffVR.mEditModeEnabled || anyGripHeld);
  mManipulation.mEditModeActive = editActive;

  const auto sendContentLeave = [&](uint8_t fallbackIdx) {
    if (!mManipulation.mHasLastContentPointer) {
      return;
    }
    mManipulation.mHasLastContentPointer = false;
    mManipulation.mWasContentTriggerDown = false;
    VRContentPointerEvent ev {};
    ev.mKneeboard = fallbackIdx;
    ev.mLayerID = mManipulation.mTargetLayerID;
    ev.mAction = VRContentPointerEvent::Action::Leave;
    APIEvent::FromStruct(ev).Send();
  };

  if (visibleLayers.empty()) {
    mManipulation.mHoveredCorner = EditCorner::None;
    mManipulation.mHoveredSurface = false;
    if (mManipulation.mDragMode != ManipulationState::DragMode::None) {
      mManipulation.mDragMode = ManipulationState::DragMode::None;
      this->SendManipulatedTransformEvent(
        mManipulation.mTargetLayerID, true, true);
    }
    mManipulation.mWasTriggerDown = false;
    mManipulation.mWasDockTriggerDown = false;
    sendContentLeave(0);
    return;
  }

  const bool canCloseOverlay = visibleLayers.size() > 1;
  mManipulation.mVisuals.mCanCloseOverlay = canCloseOverlay;

  // Build and evaluate per-overlay VR Control Bars (collapsed chevron vs expanded dock)
  {
    struct DockCandidate {
      DockBarVisual mVisual;
      const LayerSnapshot* mSnap {nullptr};
    };
    std::vector<DockCandidate> dockCandidates;
    dockCandidates.reserve(visibleLayers.size());
    for (const auto& snap : visibleLayers) {
      const auto* lSt = this->FindLayerState(snap.mSourceLayer->mLayerID);
      const bool expanded = lSt && lSt->mDockExpanded;
      Pose cDockPose {};
      Vector2 cDockSize {};
      if (expanded) {
        ComputeVRDockQuad(snap.mBoardPose, snap.mBoardSize, cDockPose, cDockSize);
      } else {
        ComputeVRDockHandleQuad(
          snap.mBoardPose, snap.mBoardSize, cDockPose, cDockSize);
      }
      DockBarVisual dbv {
        .mLayerID = snap.mSourceLayer->mLayerID,
        .mKneeboardIndex = snap.mKneeboardIndex,
        .mExpanded = expanded,
        .mPose = cDockPose,
        .mSize = cDockSize,
        .mEffectiveHeadFollow = snap.mEffectiveVR.mHeadFollow,
      };
      mManipulation.mVisuals.mDockBars.push_back(dbv);
      dockCandidates.push_back({
        .mVisual = dbv,
        .mSnap = &snap,
      });
    }

    if (!dockCandidates.empty()) {
      mManipulation.mVisuals.mHasDockBar = true;
      mManipulation.mVisuals.mDockExpanded =
        dockCandidates.front().mVisual.mExpanded;
      mManipulation.mVisuals.mDockPose = dockCandidates.front().mVisual.mPose;
      mManipulation.mVisuals.mDockSize = dockCandidates.front().mVisual.mSize;
    }

    if (mManipulation.mDragMode == ManipulationState::DragMode::None) {
      ControllerInput dockCtrl {};
      float bestDist = 0.0f;
      Vector3 bestPt {};
      VRDockButton bestBtn = VRDockButton::None;
      const DockCandidate* bestCand = nullptr;
      int bestScore = -10000;
      for (const auto& cand : dockCandidates) {
        const Vector3 dRight =
          Vector3::Transform(Vector3::UnitX, cand.mVisual.mPose.mOrientation);
        const Vector3 dUp =
          Vector3::Transform(Vector3::UnitY, cand.mVisual.mPose.mOrientation);
        const Vector3 dNormal =
          Vector3::Transform(Vector3::Backward, cand.mVisual.mPose.mOrientation);
        for (const auto& c : controllers) {
          if (!c.mValid) {
            continue;
          }
          float d = 0.0f;
          Vector3 pt {};
          if (!RayPlaneIntersection(
                c.mAimPose, cand.mVisual.mPose.mPosition, dNormal, d, pt, true)) {
            continue;
          }
          const Vector3 rel = pt - cand.mVisual.mPose.mPosition;
          const float lx = rel.Dot(dRight);
          const float ly = rel.Dot(dUp);
          if (
            std::abs(lx) > cand.mVisual.mSize.x * 0.5f
            || std::abs(ly) > cand.mVisual.mSize.y * 0.5f) {
            continue;
          }
          const float u = std::clamp(
            (lx / (std::max)(0.04f, cand.mVisual.mSize.x)) + 0.5f, 0.0f, 1.0f);
          const float v = std::clamp(
            0.5f - (ly / (std::max)(0.014f, cand.mVisual.mSize.y)), 0.0f, 1.0f);
          const VRDockButton btn = cand.mVisual.mExpanded
            ? HitTestVRDockUV(u, v, canCloseOverlay)
            : VRDockButton::Toggle;
          const int score = 1000 + (btn != VRDockButton::None ? 100 : 0)
            + (c.mTriggerHeld ? 500 : 0)
            + (c.mHandIndex == mManipulation.mActiveHandIndex ? 25 : 0)
            - static_cast<int>(d * 100.0f);
          if (score > bestScore) {
            bestScore = score;
            dockCtrl = c;
            bestDist = d;
            bestPt = pt;
            bestBtn = btn;
            bestCand = &cand;
          }
        }
      }

      for (size_t di = 0; di < dockCandidates.size(); ++di) {
        const auto& cand = dockCandidates[di];
        auto& lSt = this->GetOrCreateLayerState(
          cand.mVisual.mLayerID,
          cand.mSnap->mIsPrimary,
          config.mVR.mHeadFollow);
        if (!lSt.mDockExpanded) {
          lSt.mDockLookAwaySeconds = 0.0f;
          continue;
        }
        Pose fullPose {};
        Vector2 fullSize {};
        ComputeVRDockQuad(
          cand.mSnap->mBoardPose, cand.mSnap->mBoardSize, fullPose, fullSize);
        const bool gazeNear = cand.mSnap->mIsLooking
          || IsGazeNearOverlayOrDock(
            hmdPose, cand.mSnap->mBoardPose, cand.mSnap->mBoardSize, fullSize);
        const bool ctrlOnThis =
          dockCtrl.mValid && bestCand && bestCand->mVisual.mLayerID == cand.mVisual.mLayerID;
        if (gazeNear || ctrlOnThis) {
          lSt.mDockLookAwaySeconds = 0.0f;
        } else {
          lSt.mDockLookAwaySeconds += std::clamp(dtSeconds, 0.0f, 0.1f);
          if (lSt.mDockLookAwaySeconds >= kVRDockAutoHideDelayS) {
            lSt.mDockExpanded = false;
            lSt.mDockLookAwaySeconds = 0.0f;
            Pose handlePose {};
            Vector2 handleSize {};
            ComputeVRDockHandleQuad(
              cand.mSnap->mBoardPose,
              cand.mSnap->mBoardSize,
              handlePose,
              handleSize);
            mManipulation.mVisuals.mDockBars[di].mExpanded = false;
            mManipulation.mVisuals.mDockBars[di].mPose = handlePose;
            mManipulation.mVisuals.mDockBars[di].mSize = handleSize;
          }
        }
      }

      if (dockCtrl.mValid && bestCand) {
        sendContentLeave(bestCand->mVisual.mKneeboardIndex);
        mManipulation.mActiveHandIndex = dockCtrl.mHandIndex;
        mManipulation.mVisuals.mEditModeActive = editActive;
        mManipulation.mVisuals.mHasRay = true;
        mManipulation.mVisuals.mHoveredDockButton = bestBtn;
        mManipulation.mVisuals.mHoveredDockLayerID = bestCand->mVisual.mLayerID;
        mManipulation.mVisuals.mDockExpanded = bestCand->mVisual.mExpanded;
        mManipulation.mVisuals.mDockPose = bestCand->mVisual.mPose;
        mManipulation.mVisuals.mDockSize = bestCand->mVisual.mSize;
        auto rayDir =
          Vector3::Transform(Vector3::Forward, dockCtrl.mAimPose.mOrientation);
        if (rayDir.LengthSquared() > 1e-6f) {
          rayDir.Normalize();
        }
        mManipulation.mVisuals.mRayStartWorld = dockCtrl.mAimPose.mPosition;
        mManipulation.mVisuals.mRayEndWorld = (bestDist > 0.02f)
          ? (bestPt - (rayDir * 0.004f))
          : bestPt;

        const bool justClickedDock =
          dockCtrl.mTriggerHeld && !mManipulation.mWasDockTriggerDown;
        mManipulation.mWasDockTriggerDown = dockCtrl.mTriggerHeld;
        if (justClickedDock && bestBtn != VRDockButton::None) {
          this->HandleVRDockButtonClick(
            bestBtn,
            canCloseOverlay,
            bestCand->mSnap->mBoardPose,
            bestCand->mSnap->mBoardSize.y,
            hmdPose,
            handMountPoses,
            bestCand->mSnap->mWorkingLayer,
            bestCand->mVisual.mKneeboardIndex,
            bestCand->mSnap->mIsPrimary,
            config);
        } else if (
          bestCand->mVisual.mExpanded
          && std::abs(dockCtrl.mThumbstickY) > 0.25f) {
          auto& lSt = this->GetOrCreateLayerState(
            bestCand->mVisual.mLayerID,
            bestCand->mSnap->mIsPrimary,
            config.mVR.mHeadFollow);
          lSt.mLastSeenSHMHeadFollow = config.mVR.mHeadFollow;
          if (!lSt.mHasOptimisticPose) {
            lSt.mOptimisticWorldPose = bestCand->mSnap->mWorkingLayer.mVR.mPose;
          }
          mManipulation.mTargetLayerID = bestCand->mVisual.mLayerID;
          mManipulation.mTargetKneeboardIndex =
            bestCand->mVisual.mKneeboardIndex;
          auto& hfDock = lSt.mOptimisticHeadFollow;
          const auto effDock = GetEffectiveFollowMode(hfDock);
          const float deltaM = dockCtrl.mThumbstickY * 0.45f * dtSeconds;
          if (
            effDock == VRFollowMode::LeftHand
            || effDock == VRFollowMode::RightHand) {
            SetHandDistanceMeters(hfDock, GetHandDistanceMeters(hfDock) + deltaM);
          } else if (effDock != VRFollowMode::WorldLocked) {
            hfDock.mHeadOffsetZ =
              std::clamp(hfDock.mHeadOffsetZ - deltaM, -1.35f, -0.20f);
          }
          lSt.mHasOptimisticVR = true;
          lSt.mHasOptimisticPose = true;
          lSt.mLastChangeTime = std::chrono::steady_clock::now();
          this->SendManipulatedTransformEvent(
            bestCand->mVisual.mLayerID, bestCand->mSnap->mIsPrimary, false);
        }
        return;
      }
      mManipulation.mWasDockTriggerDown = false;
    }
  }

  // Non-edit content pointer raycast (front-face only)
  if (!editActive) {
    mManipulation.mHoveredCorner = EditCorner::None;
    mManipulation.mHoveredSurface = false;
    if (mManipulation.mDragMode != ManipulationState::DragMode::None) {
      mManipulation.mDragMode = ManipulationState::DragMode::None;
      this->SendManipulatedTransformEvent(
        mManipulation.mTargetLayerID, true, true);
    }
    mManipulation.mWasTriggerDown = false;

    const LayerSnapshot* hitSnap = nullptr;
    ControllerInput pointerCtrl {};
    float bestHitDist = 1e9f;
    Vector3 bestHitPt {};
    float bestU = 0.5f;
    float bestV = 0.5f;
    int bestScore = -10000;
    for (const auto& snap : visibleLayers) {
      const auto cRight =
        Vector3::Transform(Vector3::UnitX, snap.mBoardPose.mOrientation);
      const auto cUp =
        Vector3::Transform(Vector3::UnitY, snap.mBoardPose.mOrientation);
      const auto cNormal =
        Vector3::Transform(Vector3::Backward, snap.mBoardPose.mOrientation);
      for (const auto& c : controllers) {
        if (!c.mValid) {
          continue;
        }
        float d = 0.0f;
        Vector3 pt {};
        if (!RayPlaneIntersection(
              c.mAimPose, snap.mBoardPose.mPosition, cNormal, d, pt, true)) {
          continue;
        }
        const Vector3 rel = pt - snap.mBoardPose.mPosition;
        const float lx = rel.Dot(cRight);
        const float ly = rel.Dot(cUp);
        if (
          std::abs(lx) > snap.mBoardSize.x * 0.5f
          || std::abs(ly) > snap.mBoardSize.y * 0.5f) {
          continue;
        }
        int score = 1000 + (c.mTriggerHeld ? 500 : 0)
          + (c.mHandIndex == mManipulation.mActiveHandIndex ? 25 : 0)
          - static_cast<int>(d * 100.0f);
        if (score > bestScore) {
          bestScore = score;
          hitSnap = &snap;
          pointerCtrl = c;
          bestHitDist = d;
          bestHitPt = pt;
          bestU = std::clamp(
            (lx / (std::max)(0.05f, snap.mBoardSize.x)) + 0.5f, 0.0f, 1.0f);
          bestV = std::clamp(
            0.5f - (ly / (std::max)(0.05f, snap.mBoardSize.y)), 0.0f, 1.0f);
        }
      }
    }

    if (!hitSnap || !pointerCtrl.mValid) {
      sendContentLeave(visibleLayers.front().mKneeboardIndex);
      return;
    }

    auto rayDir = Vector3::Transform(
      Vector3::Forward, pointerCtrl.mAimPose.mOrientation);
    if (rayDir.LengthSquared() > 1e-6f) {
      rayDir.Normalize();
      auto& vis = mManipulation.mVisuals;
      vis.mEditModeActive = false;
      vis.mContentPointerActive = true;
      vis.mHasRay = true;
      vis.mHasHitOnBoard = true;
      vis.mTargetLayerID = hitSnap->mSourceLayer->mLayerID;
      vis.mHitLocalU = bestU;
      vis.mHitLocalV = bestV;
      vis.mRayStartWorld = pointerCtrl.mAimPose.mPosition;
      vis.mRayEndWorld =
        (bestHitDist > 0.02f)
          ? (bestHitPt - (rayDir * 0.004f))
          : bestHitPt;
    }

    mManipulation.mActiveHandIndex = pointerCtrl.mHandIndex;
    mManipulation.mTargetLayerID = hitSnap->mSourceLayer->mLayerID;
    mManipulation.mTargetKneeboardIndex = hitSnap->mKneeboardIndex;
    const auto nowPtr = std::chrono::steady_clock::now();
    const bool triggerDown = pointerCtrl.mTriggerHeld;
    const bool wasDown = mManipulation.mWasContentTriggerDown;
    const uint32_t buttons = triggerDown ? 1u : 0u;
    const bool uvChanged = !mManipulation.mHasLastContentPointer
      || std::abs(bestU - mManipulation.mLastContentU) > 0.0015f
      || std::abs(bestV - mManipulation.mLastContentV) > 0.0015f;
    const bool moveThrottle =
      (nowPtr - mManipulation.mLastContentSendTime)
      >= std::chrono::milliseconds(18);

    if (triggerDown != wasDown) {
      VRContentPointerEvent ev {};
      ev.mKneeboard = hitSnap->mKneeboardIndex;
      ev.mLayerID = hitSnap->mSourceLayer->mLayerID;
      ev.mAction = triggerDown
        ? VRContentPointerEvent::Action::PrimaryDown
        : VRContentPointerEvent::Action::PrimaryUp;
      ev.mU = bestU;
      ev.mV = bestV;
      ev.mButtons = buttons;
      APIEvent::FromStruct(ev).Send();
      mManipulation.mLastContentSendTime = nowPtr;
      mManipulation.mLastContentU = bestU;
      mManipulation.mLastContentV = bestV;
      mManipulation.mHasLastContentPointer = true;
    } else if (uvChanged && moveThrottle) {
      VRContentPointerEvent ev {};
      ev.mKneeboard = hitSnap->mKneeboardIndex;
      ev.mLayerID = hitSnap->mSourceLayer->mLayerID;
      ev.mAction = VRContentPointerEvent::Action::Move;
      ev.mU = bestU;
      ev.mV = bestV;
      ev.mButtons = buttons;
      APIEvent::FromStruct(ev).Send();
      mManipulation.mLastContentSendTime = nowPtr;
      mManipulation.mLastContentU = bestU;
      mManipulation.mLastContentV = bestV;
      mManipulation.mHasLastContentPointer = true;
    }
    mManipulation.mWasContentTriggerDown = triggerDown;

    if (
      std::abs(pointerCtrl.mThumbstickY) > 0.25f
      && (nowPtr - mManipulation.mLastContentWheelTime)
        >= std::chrono::milliseconds(60)) {
      VRContentPointerEvent ev {};
      ev.mKneeboard = hitSnap->mKneeboardIndex;
      ev.mLayerID = hitSnap->mSourceLayer->mLayerID;
      ev.mAction = VRContentPointerEvent::Action::Wheel;
      ev.mU = bestU;
      ev.mV = bestV;
      ev.mButtons = buttons;
      ev.mWheelDeltaY = static_cast<int32_t>(
        std::lround(pointerCtrl.mThumbstickY * 120.0f));
      APIEvent::FromStruct(ev).Send();
      mManipulation.mLastContentWheelTime = nowPtr;
      mManipulation.mHasLastContentPointer = true;
    }
    return;
  }
  sendContentLeave(visibleLayers.front().mKneeboardIndex);

  // Edit Mode target selection:
  // If actively dragging, keep locked on mManipulation.mTargetLayerID.
  // Otherwise, activate Edit Mode STRICTLY on the single front-facing overlay under the HMD gaze center.
  const LayerSnapshot* targetSnap = nullptr;
  if (mManipulation.mDragMode != ManipulationState::DragMode::None) {
    for (const auto& snap : visibleLayers) {
      if (snap.mSourceLayer->mLayerID == mManipulation.mTargetLayerID) {
        targetSnap = &snap;
        break;
      }
    }
    if (!targetSnap) {
      mManipulation.mDragMode = ManipulationState::DragMode::None;
      this->SendManipulatedTransformEvent(
        mManipulation.mTargetLayerID, true, true);
    }
  } else {
    float bestGazeDist = 1e9f;
    for (const auto& snap : visibleLayers) {
      const Vector3 sNormal =
        Vector3::Transform(Vector3::Backward, snap.mBoardPose.mOrientation);
      float gd = 0.0f;
      Vector3 gpt {};
      if (!RayPlaneIntersection(
            hmdPose, snap.mBoardPose.mPosition, sNormal, gd, gpt, true)) {
        continue;
      }
      const Vector3 sRight =
        Vector3::Transform(Vector3::UnitX, snap.mBoardPose.mOrientation);
      const Vector3 sUp =
        Vector3::Transform(Vector3::UnitY, snap.mBoardPose.mOrientation);
      const Vector3 rel = gpt - snap.mBoardPose.mPosition;
      const float lx = rel.Dot(sRight);
      const float ly = rel.Dot(sUp);
      if (
        std::abs(lx) <= (snap.mBoardSize.x * 0.5f) + 0.015f
        && std::abs(ly) <= (snap.mBoardSize.y * 0.5f) + 0.015f
        && gd < bestGazeDist) {
        bestGazeDist = gd;
        targetSnap = &snap;
      }
    }
  }

  if (!targetSnap) {
    mManipulation.mHoveredCorner = EditCorner::None;
    mManipulation.mHoveredSurface = false;
    bool anyTrigger = false;
    for (const auto& c : controllers) {
      if (c.mValid && c.mTriggerHeld) {
        anyTrigger = true;
        break;
      }
    }
    mManipulation.mWasTriggerDown = anyTrigger;
    mManipulation.mVisuals.mEditModeActive = true;
    mManipulation.mVisuals.mTargetLayerID = 0;
    return;
  }

  auto& lState = this->GetOrCreateLayerState(
    targetSnap->mSourceLayer->mLayerID,
    targetSnap->mIsPrimary,
    config.mVR.mHeadFollow);
  const auto effectiveVR = targetSnap->mEffectiveVR;
  const auto currentEffMode = GetEffectiveFollowMode(effectiveVR.mHeadFollow);
  const std::optional<Pose> handMountPose =
    (currentEffMode == VRFollowMode::LeftHand)
      ? handMountPoses[0]
      : ((currentEffMode == VRFollowMode::RightHand) ? handMountPoses[1]
                                                     : std::nullopt);
  const SHM::LayerConfig& workingLayer = targetSnap->mWorkingLayer;
  const uint8_t targetIndex = targetSnap->mKneeboardIndex;
  const bool isPrimaryTarget = targetSnap->mIsPrimary;
  const bool isLooking = targetSnap->mIsLooking;
  const bool isZoomed = effectiveVR.mForceZoom
    || (isLooking && workingLayer.mVR.mEnableGazeZoom);
  const float zoomFactor = isZoomed
    ? (std::max)(0.25f, workingLayer.mVR.mZoomScale)
    : 1.0f;
  Vector2 boardSize = targetSnap->mBoardSize;
  Pose boardPose = targetSnap->mBoardPose;

  const auto boardRight =
    Vector3::Transform(Vector3::UnitX, boardPose.mOrientation);
  const auto boardUp = Vector3::Transform(Vector3::UnitY, boardPose.mOrientation);
  const auto boardNormal =
    Vector3::Transform(Vector3::Backward, boardPose.mOrientation);

  ControllerInput controller {};
  if (mManipulation.mDragMode != ManipulationState::DragMode::None) {
    for (const auto& c : controllers) {
      if (c.mValid && c.mHandIndex == mManipulation.mActiveHandIndex) {
        controller = c;
        break;
      }
    }
  }
  if (!controller.mValid) {
    int bestScore = -1;
    for (const auto& c : controllers) {
      if (!c.mValid) {
        continue;
      }
      int score = 0;
      if (c.mTriggerHeld) {
        score += 100;
      }
      if (c.mGripHeld) {
        score += 50;
      }
      float d = 0.0f;
      Vector3 pt {};
      if (RayPlaneIntersection(
            c.mAimPose, boardPose.mPosition, boardNormal, d, pt, true)) {
        const Vector3 rel = pt - boardPose.mPosition;
        const float lx = rel.Dot(boardRight);
        const float ly = rel.Dot(boardUp);
        if (
          std::abs(lx) <= (boardSize.x * 0.5f) + 0.015f
          && std::abs(ly) <= (boardSize.y * 0.5f) + 0.015f) {
          score += 20;
        }
      }
      if (std::abs(c.mThumbstickY) > 0.25f) {
        score += 5;
      }
      if (score > bestScore) {
        bestScore = score;
        controller = c;
      }
    }
  }

  mManipulation.mHoveredCorner = EditCorner::None;
  mManipulation.mHoveredSurface = false;

  if (
    mManipulation.mDragMode == ManipulationState::DragMode::None
    && controller.mValid) {
    float hitDist = 0.0f;
    Vector3 hitPoint {};
    if (RayPlaneIntersection(
          controller.mAimPose, boardPose.mPosition, boardNormal, hitDist, hitPoint, true)) {
      const Vector3 rel = hitPoint - boardPose.mPosition;
      const float localX = rel.Dot(boardRight);
      const float localY = rel.Dot(boardUp);
      const float halfW = boardSize.x * 0.5f;
      const float halfH = boardSize.y * 0.5f;
      const float cornerRadius =
        (std::max)(0.026f, (std::min)(boardSize.x, boardSize.y) * 0.12f);

      struct Candidate {
        EditCorner mCorner;
        float mX;
        float mY;
      };
      const std::array<Candidate, 4> corners {{
        {EditCorner::NW, -halfW, halfH},
        {EditCorner::NE, halfW, halfH},
        {EditCorner::SW, -halfW, -halfH},
        {EditCorner::SE, halfW, -halfH},
      }};
      float bestDist = cornerRadius;
      for (const auto& c : corners) {
        const float d = std::hypot(localX - c.mX, localY - c.mY);
        if (d <= bestDist) {
          bestDist = d;
          mManipulation.mHoveredCorner = c.mCorner;
        }
      }
      if (
        mManipulation.mHoveredCorner != EditCorner::None
        || (std::abs(localX) <= halfW + 0.015f
            && std::abs(localY) <= halfH + 0.015f)) {
        mManipulation.mHoveredSurface = true;
      }

      const bool justPressed =
        controller.mTriggerHeld && !mManipulation.mWasTriggerDown;
      if (justPressed && mManipulation.mHoveredSurface) {
        mManipulation.mActiveHandIndex = controller.mHandIndex;
        mManipulation.mTargetLayerID = workingLayer.mLayerID;
        mManipulation.mTargetKneeboardIndex = targetIndex;
        mManipulation.mGrabDistanceM = hitDist;
        mManipulation.mGrabLocalU =
          std::clamp((localX / (std::max)(0.05f, boardSize.x)) + 0.5f, 0.0f, 1.0f);
        mManipulation.mGrabLocalV =
          std::clamp(0.5f - (localY / (std::max)(0.05f, boardSize.y)), 0.0f, 1.0f);
        mManipulation.mGrabBoardStartOrient = boardPose.mOrientation;
        mManipulation.mGrabStartSize = boardSize;
        mManipulation.mGrabZoomFactor = zoomFactor;
        if (!lState.mHasOptimisticVR) {
          lState.mOptimisticHeadFollow = effectiveVR.mHeadFollow;
        }
        lState.mLastSeenSHMHeadFollow = config.mVR.mHeadFollow;
        if (!lState.mHasOptimisticPose) {
          lState.mOptimisticWorldPose = workingLayer.mVR.mPose;
        }
        if (!lState.mHasOptimisticSize) {
          lState.mOptimisticSize = {
            boardSize.x / zoomFactor,
            boardSize.y / zoomFactor,
          };
        }

        if (mManipulation.mHoveredCorner != EditCorner::None) {
          switch (mManipulation.mHoveredCorner) {
            case EditCorner::NW:
              mManipulation.mDragMode = ManipulationState::DragMode::ResizeNW;
              break;
            case EditCorner::NE:
              mManipulation.mDragMode = ManipulationState::DragMode::ResizeNE;
              break;
            case EditCorner::SW:
              mManipulation.mDragMode = ManipulationState::DragMode::ResizeSW;
              break;
            case EditCorner::SE:
              mManipulation.mDragMode = ManipulationState::DragMode::ResizeSE;
              break;
            default:
              break;
          }
          const auto [u, v] = GetCornerSigns(mManipulation.mHoveredCorner);
          mManipulation.mGrabAnchorCornerWorld = boardPose.mPosition
            - (boardRight * (u * halfW))
            - (boardUp * (v * halfH));
        } else {
          mManipulation.mDragMode = ManipulationState::DragMode::Move;
          mManipulation.mGrabPositionOffset = boardPose.mPosition - hitPoint;
          Quaternion invCtrl = controller.mAimPose.mOrientation;
          invCtrl.Inverse(invCtrl);
          mManipulation.mGrabControllerInv = invCtrl;
          const auto effectiveStartMode =
            GetEffectiveFollowMode(lState.mOptimisticHeadFollow);
          if (IsHandPinnedMode(effectiveStartMode) && handMountPose) {
            const Vector3 bottomPos0 =
              ExtractHandBottomEdgePosition(boardPose, boardSize.y);
            const Vector3 radial0 = bottomPos0 - handMountPose->mPosition;
            const float radius0 = std::clamp(
              radial0.Length(), kHandDistanceMinM, kHandDistanceMaxM);
            mManipulation.mGrabHandRadiusM = radius0;
            const Vector3 boardDir0 = (radial0.LengthSquared() > 1e-8f)
              ? (radial0 / radial0.Length())
              : Vector3::UnitY;
            auto rayDir0 = Vector3::Transform(
              Vector3::Forward, controller.mAimPose.mOrientation);
            if (rayDir0.LengthSquared() > 1e-8f) {
              rayDir0.Normalize();
            } else {
              rayDir0 = Vector3::Forward;
            }
            const float initialGrabR = ComputeWristRadialDragRadius(
              controller.mAimPose.mPosition,
              rayDir0,
              hitDist,
              handMountPose->mPosition,
              radius0);
            mManipulation.mGrabHandRadialDeltaM = radius0 - initialGrabR;
            const Vector3 grabDir0 = ComputeWristOrbitDragPointDir(
              controller.mAimPose.mPosition,
              rayDir0,
              hitDist,
              handMountPose->mPosition,
              boardDir0);
            mManipulation.mGrabHandOrbitOffset =
              QuatFromTo(grabDir0, boardDir0);
          } else {
            mManipulation.mGrabHandOrbitOffset = Quaternion::Identity;
          }
        }
      }
    }
  } else if (mManipulation.mDragMode != ManipulationState::DragMode::None) {
    if (!controller.mValid || !controller.mTriggerHeld) {
      mManipulation.mDragMode = ManipulationState::DragMode::None;
      this->SendManipulatedTransformEvent(
        workingLayer.mLayerID, isPrimaryTarget, true);
    } else if (mManipulation.mDragMode == ManipulationState::DragMode::Move) {
      const bool frontFacingDrag =
        IsFrontFacingBoardAndRay(
          controller.mAimPose, boardPose.mPosition, boardNormal)
        && ((hmdPose.mPosition - boardPose.mPosition).Dot(boardNormal) > 1e-4f);
      if (frontFacingDrag) {
        const auto effectiveDragMode =
          GetEffectiveFollowMode(lState.mOptimisticHeadFollow);
        const bool isHandPivotDrag =
          (effectiveDragMode == VRFollowMode::LeftHand
           || effectiveDragMode == VRFollowMode::RightHand)
          && handMountPose;

        auto rayDir =
          Vector3::Transform(Vector3::Forward, controller.mAimPose.mOrientation);
        if (rayDir.LengthSquared() > 1e-8f) {
          rayDir.Normalize();
        } else {
          rayDir = Vector3::Forward;
        }

        Vector3 targetPos {};
        Quaternion targetOrient {};
        if (isHandPivotDrag) {
          if (std::abs(controller.mThumbstickY) > 0.18f) {
            const float stickDelta = controller.mThumbstickY * 0.45f * dtSeconds;
            mManipulation.mGrabHandRadialDeltaM += stickDelta;
            mManipulation.mGrabDistanceM = std::clamp(
              mManipulation.mGrabDistanceM + stickDelta, 0.08f, 2.5f);
          }
          const float rawGrabRadius = ComputeWristRadialDragRadius(
            controller.mAimPose.mPosition,
            rayDir,
            mManipulation.mGrabDistanceM,
            handMountPose->mPosition,
            mManipulation.mGrabHandRadiusM);
          mManipulation.mGrabHandRadiusM = std::clamp(
            rawGrabRadius + mManipulation.mGrabHandRadialDeltaM,
            kHandDistanceMinM,
            kHandDistanceMaxM);
          const Vector3 rawDir = ComputeWristOrbitDragPointDir(
            controller.mAimPose.mPosition,
            rayDir,
            mManipulation.mGrabDistanceM,
            handMountPose->mPosition,
            Vector3::UnitY);
          Vector3 boardDir =
            Vector3::Transform(rawDir, mManipulation.mGrabHandOrbitOffset);
          if (boardDir.LengthSquared() > 1e-8f) {
            boardDir.Normalize();
          } else {
            boardDir = rawDir;
          }
          const Vector3 targetBottomPos = handMountPose->mPosition
            + (boardDir * mManipulation.mGrabHandRadiusM);
          const Vector3 prevForward = boardNormal;
          const Quaternion axisOrient = ComputeWristAxisOrientation(
            targetBottomPos,
            handMountPose->mPosition,
            hmdPose.mPosition,
            handMountPose->mOrientation,
            &prevForward);
          const float tiltDeg = lState.mOptimisticHeadFollow.mWristTiltOffset;
          targetOrient = (std::abs(tiltDeg) > 1e-4f)
            ? (Quaternion::CreateFromAxisAngle(
                 Vector3::UnitX, DirectX::XMConvertToRadians(tiltDeg))
               * axisOrient)
            : axisOrient;
          targetPos = ApplyHandBottomEdgeAnchor(
            {.mPosition = targetBottomPos, .mOrientation = targetOrient},
            boardSize.y)
            .mPosition;
          mManipulation.mGrabDistanceM = std::clamp(
            (targetBottomPos - controller.mAimPose.mPosition).Length(), 0.06f, 2.5f);
        } else {
          if (std::abs(controller.mThumbstickY) > 0.18f) {
            mManipulation.mGrabDistanceM = std::clamp(
              mManipulation.mGrabDistanceM
                + (controller.mThumbstickY * 0.65f * dtSeconds),
              0.22f,
              1.45f);
          }
          const Vector3 targetHit =
            controller.mAimPose.mPosition + (rayDir * mManipulation.mGrabDistanceM);
          const Quaternion deltaRot =
            mManipulation.mGrabControllerInv * controller.mAimPose.mOrientation;
          targetOrient = mManipulation.mGrabBoardStartOrient * deltaRot;
          const Vector3 targetRight =
            Vector3::Transform(Vector3::UnitX, targetOrient);
          const Vector3 targetUp =
            Vector3::Transform(Vector3::UnitY, targetOrient);
          const float offsetX = (mManipulation.mGrabLocalU - 0.5f) * boardSize.x;
          const float offsetY = (0.5f - mManipulation.mGrabLocalV) * boardSize.y;
          targetPos = targetHit
            - (targetRight * offsetX)
            - (targetUp * offsetY);
        }
        const Pose targetPose {
          .mPosition = targetPos,
          .mOrientation = targetOrient,
        };
        lState.mHasOptimisticVR = true;
        lState.mHasOptimisticPose = true;
        this->SolvePoseIntoSettings(
          lState.mOptimisticHeadFollow,
          lState.mOptimisticWorldPose,
          targetPose,
          hmdPose,
          handMountPose,
          boardSize.y);
        lState.mLastChangeTime = std::chrono::steady_clock::now();
        this->SendManipulatedTransformEvent(
          workingLayer.mLayerID, isPrimaryTarget, false);
      }
    } else {
      EditCorner corner = EditCorner::SE;
      switch (mManipulation.mDragMode) {
        case ManipulationState::DragMode::ResizeNW:
          corner = EditCorner::NW;
          break;
        case ManipulationState::DragMode::ResizeNE:
          corner = EditCorner::NE;
          break;
        case ManipulationState::DragMode::ResizeSW:
          corner = EditCorner::SW;
          break;
        case ManipulationState::DragMode::ResizeSE:
          corner = EditCorner::SE;
          break;
        default:
          break;
      }
      const auto [u, v] = GetCornerSigns(corner);
      const auto anchorRight =
        Vector3::Transform(Vector3::UnitX, mManipulation.mGrabBoardStartOrient);
      const auto anchorUp =
        Vector3::Transform(Vector3::UnitY, mManipulation.mGrabBoardStartOrient);
      const auto anchorNormal = Vector3::Transform(
        Vector3::Backward, mManipulation.mGrabBoardStartOrient);

      float hitDist = 0.0f;
      Vector3 hitWorld {};
      if (RayPlaneIntersection(
            controller.mAimPose,
            mManipulation.mGrabAnchorCornerWorld,
            anchorNormal,
            hitDist,
            hitWorld,
            true)) {
        const Vector3 delta = hitWorld - mManipulation.mGrabAnchorCornerWorld;
        const float rawW = delta.Dot(anchorRight) * u;
        const float rawH = delta.Dot(anchorUp) * v;
        const float aspect = mManipulation.mGrabStartSize.x
          / (std::max)(0.05f, mManipulation.mGrabStartSize.y);
        const float grabZ = (std::max)(0.25f, mManipulation.mGrabZoomFactor);
        const float invZ = 1.0f / grabZ;
        float newH = std::clamp(
          (rawH + (rawW / aspect)) * 0.5f, 0.12f * grabZ, 0.85f * grabZ);
        float newW = std::clamp(newH * aspect, 0.12f * grabZ, 0.85f * grabZ);
        newH = newW / aspect;

        const float unzoomedW = std::clamp(newW * invZ, 0.10f, 0.85f);
        const float unzoomedH = std::clamp(newH * invZ, 0.10f, 0.85f);
        newW = unzoomedW * grabZ;
        newH = unzoomedH * grabZ;
        boardSize = {newW, newH};

        const Vector3 newCenter = mManipulation.mGrabAnchorCornerWorld
          + (anchorRight * (u * newW * 0.5f))
          + (anchorUp * (v * newH * 0.5f));
        const Pose targetPose {
          .mPosition = newCenter,
          .mOrientation = mManipulation.mGrabBoardStartOrient,
        };
        lState.mHasOptimisticVR = true;
        lState.mHasOptimisticPose = true;
        lState.mHasOptimisticSize = true;
        lState.mOptimisticSize = {unzoomedW, unzoomedH};
        this->SolvePoseIntoSettings(
          lState.mOptimisticHeadFollow,
          lState.mOptimisticWorldPose,
          targetPose,
          hmdPose,
          handMountPose,
          newH);
        lState.mLastChangeTime = std::chrono::steady_clock::now();
        this->SendManipulatedTransformEvent(
          workingLayer.mLayerID, isPrimaryTarget, false);
      }
    }
  }

  // Thumbstick depth nudge while hovering in Edit Mode without dragging
  if (
    mManipulation.mDragMode == ManipulationState::DragMode::None
    && controller.mValid && mManipulation.mHoveredSurface
    && std::abs(controller.mThumbstickY) > 0.25f) {
    if (!lState.mHasOptimisticVR) {
      lState.mOptimisticHeadFollow = effectiveVR.mHeadFollow;
    }
    lState.mLastSeenSHMHeadFollow = config.mVR.mHeadFollow;
    if (!lState.mHasOptimisticPose) {
      lState.mOptimisticWorldPose = workingLayer.mVR.mPose;
    }
    mManipulation.mTargetLayerID = workingLayer.mLayerID;
    mManipulation.mTargetKneeboardIndex = targetIndex;

    auto& hf = lState.mOptimisticHeadFollow;
    const auto effectiveMode = GetEffectiveFollowMode(hf);
    const float deltaM = controller.mThumbstickY * 0.55f * dtSeconds;
    if (effectiveMode == VRFollowMode::WorldLocked) {
      Vector3 radial = boardPose.mPosition - hmdPose.mPosition;
      const float len = radial.Length();
      if (len > 0.05f) {
        radial /= len;
        const float nextLen = std::clamp(len + deltaM, 0.22f, 1.45f);
        Pose targetPose = boardPose;
        targetPose.mPosition = hmdPose.mPosition + (radial * nextLen);
        this->SolvePoseIntoSettings(
          hf,
          lState.mOptimisticWorldPose,
          targetPose,
          hmdPose,
          handMountPose);
      }
    } else if (
      effectiveMode == VRFollowMode::LeftHand
      || effectiveMode == VRFollowMode::RightHand) {
      const Vector3 v(hf.mHandOffsetX, hf.mHandOffsetY, hf.mHandOffsetZ);
      const float curR = v.Length();
      const Vector3 dir = (curR > 1e-4f) ? (v / curR) : Vector3::UnitY;
      const float nextR =
        std::clamp(((curR > 1e-4f) ? curR : 0.10f) + deltaM, 0.06f, 0.65f);
      hf.mHandOffsetX = std::clamp(dir.x * nextR, -0.65f, 0.65f);
      hf.mHandOffsetY = std::clamp(dir.y * nextR, -0.45f, 0.65f);
      hf.mHandOffsetZ = std::clamp(dir.z * nextR, -0.65f, 0.65f);
    } else {
      hf.mHeadOffsetZ = std::clamp(hf.mHeadOffsetZ - deltaM, -1.35f, -0.20f);
    }
    lState.mHasOptimisticVR = true;
    lState.mHasOptimisticPose = true;
    lState.mLastChangeTime = std::chrono::steady_clock::now();
    this->SendManipulatedTransformEvent(
      workingLayer.mLayerID, isPrimaryTarget, false);
  }

  mManipulation.mWasTriggerDown = controller.mTriggerHeld;

  auto& vis = mManipulation.mVisuals;
  vis.mEditModeActive = true;
  vis.mDragging =
    mManipulation.mDragMode != ManipulationState::DragMode::None;
  vis.mHoveredCorner = mManipulation.mHoveredCorner;
  vis.mTargetLayerID = workingLayer.mLayerID;
  if (
    vis.mDragging
    && mManipulation.mDragMode != ManipulationState::DragMode::Move) {
    switch (mManipulation.mDragMode) {
      case ManipulationState::DragMode::ResizeNW:
        vis.mHoveredCorner = EditCorner::NW;
        break;
      case ManipulationState::DragMode::ResizeNE:
        vis.mHoveredCorner = EditCorner::NE;
        break;
      case ManipulationState::DragMode::ResizeSW:
        vis.mHoveredCorner = EditCorner::SW;
        break;
      case ManipulationState::DragMode::ResizeSE:
        vis.mHoveredCorner = EditCorner::SE;
        break;
      default:
        break;
    }
  }

  if (controller.mValid) {
    auto rayDir =
      Vector3::Transform(Vector3::Forward, controller.mAimPose.mOrientation);
    if (rayDir.LengthSquared() > 1e-6f) {
      rayDir.Normalize();
      vis.mRayStartWorld = controller.mAimPose.mPosition;

      SHM::LayerConfig visLayer = workingLayer;
      if (lState.mHasOptimisticPose) {
        visLayer.mVR.mPose = lState.mOptimisticWorldPose;
      }
      const auto visEffectiveVR = this->GetEffectiveLayerVRSettings(
        config.mVR, workingLayer.mLayerID, isPrimaryTarget);
      const auto visEffMode = GetEffectiveFollowMode(visEffectiveVR.mHeadFollow);
      const std::optional<Pose> visHandPose =
        (visEffMode == VRFollowMode::LeftHand)
          ? handMountPoses[0]
          : ((visEffMode == VRFollowMode::RightHand) ? handMountPoses[1]
                                                     : std::nullopt);
      const Pose visBasePose = this->GetKneeboardPose(
        visEffectiveVR, visLayer, hmdPose, 0.0f, visHandPose);
      const Pose visBoardPose = IsHandPinnedMode(visEffMode)
        ? ApplyHandBottomEdgeAnchor(visBasePose, boardSize.y)
        : visBasePose;

      for (auto& db : vis.mDockBars) {
        if (db.mLayerID == workingLayer.mLayerID) {
          if (db.mExpanded) {
            ComputeVRDockQuad(visBoardPose, boardSize, db.mPose, db.mSize);
          } else {
            ComputeVRDockHandleQuad(visBoardPose, boardSize, db.mPose, db.mSize);
          }
          db.mEffectiveHeadFollow = visEffectiveVR.mHeadFollow;
          vis.mDockPose = db.mPose;
          vis.mDockSize = db.mSize;
          vis.mDockExpanded = db.mExpanded;
          break;
        }
      }

      const Vector3 visRight =
        Vector3::Transform(Vector3::UnitX, visBoardPose.mOrientation);
      const Vector3 visUp =
        Vector3::Transform(Vector3::UnitY, visBoardPose.mOrientation);

      if (mManipulation.mDragMode == ManipulationState::DragMode::Move) {
        vis.mHasRay = true;
        vis.mHasHitOnBoard = true;
        vis.mHitLocalU = mManipulation.mGrabLocalU;
        vis.mHitLocalV = mManipulation.mGrabLocalV;
        const float visOffX = (vis.mHitLocalU - 0.5f) * boardSize.x;
        const float visOffY = (0.5f - vis.mHitLocalV) * boardSize.y;
        const Vector3 grabWorld = visBoardPose.mPosition
          + (visRight * visOffX) + (visUp * visOffY);
        const Vector3 beamDelta = grabWorld - vis.mRayStartWorld;
        const float beamDist = beamDelta.Length();
        vis.mRayEndWorld = (beamDist > 0.04f)
          ? (vis.mRayStartWorld + (beamDelta / beamDist) * (beamDist - 0.004f))
          : grabWorld;
      } else if (vis.mDragging) {
        vis.mHasRay = true;
        const auto [u, v] = GetCornerSigns(vis.mHoveredCorner);
        vis.mHasHitOnBoard = true;
        vis.mHitLocalU = (u > 0.0f) ? 1.0f : 0.0f;
        vis.mHitLocalV = (v > 0.0f) ? 0.0f : 1.0f;
        const Vector3 cornerWorld = visBoardPose.mPosition
          + (visRight * (u * boardSize.x * 0.5f))
          + (visUp * (v * boardSize.y * 0.5f));
        const Vector3 beamDelta = cornerWorld - vis.mRayStartWorld;
        const float beamDist = beamDelta.Length();
        vis.mRayEndWorld = (beamDist > 0.04f)
          ? (vis.mRayStartWorld + (beamDelta / beamDist) * (beamDist - 0.004f))
          : cornerWorld;
      } else {
        float hitDist = 0.0f;
        Vector3 hitPt {};
        if (RayPlaneIntersection(
              controller.mAimPose,
              boardPose.mPosition,
              boardNormal,
              hitDist,
              hitPt,
              true)) {
          const Vector3 rel = hitPt - boardPose.mPosition;
          const float localX = rel.Dot(boardRight);
          const float localY = rel.Dot(boardUp);
          const float halfW = boardSize.x * 0.5f;
          const float halfH = boardSize.y * 0.5f;
          if (
            (std::abs(localX) <= halfW + 0.015f
             && std::abs(localY) <= halfH + 0.015f)
            || mManipulation.mHoveredCorner != EditCorner::None) {
            vis.mHasRay = true;
            vis.mHasHitOnBoard = true;
            vis.mHitLocalU = std::clamp(
              (localX / (std::max)(0.05f, boardSize.x)) + 0.5f, 0.0f, 1.0f);
            vis.mHitLocalV = std::clamp(
              0.5f - (localY / (std::max)(0.05f, boardSize.y)), 0.0f, 1.0f);
            vis.mRayEndWorld = hitPt - (rayDir * 0.004f);
          }
        }
      }
    }
  }
}

VRKneeboard::Pose VRKneeboard::GetKneeboardPose(
  const VRRenderSettings& vr,
  const SHM::LayerConfig& layer,
  const Pose& hmdPose,
  const float dtSeconds,
  const std::optional<Pose>& handMountPose) {
  if (!mEyeHeight) {
    mEyeHeight = {hmdPose.mPosition.y};
  }
  const auto effectiveMode = GetEffectiveFollowMode(vr.mHeadFollow);
  const auto& pose = layer.mVR.mPose;
  this->MaybeRecenter(vr, hmdPose);
  if (effectiveMode == VRFollowMode::WorldLocked) {
    const auto matrix = Matrix::CreateRotationX(pose.mRX)
      * Matrix::CreateRotationY(pose.mRY) * Matrix::CreateRotationZ(pose.mRZ)
      * Matrix::CreateTranslation({
        pose.mX,
        pose.mEyeY + *mEyeHeight,
        pose.mZ,
      })
      * mRecenter;
    auto& state = mFollowStates[layer.mLayerID];
    state.mLastEffectiveMode = effectiveMode;
    return {
      .mPosition = matrix.Translation(),
      .mOrientation = Quaternion::CreateFromRotationMatrix(matrix),
    };
  }

  auto& state = mFollowStates[layer.mLayerID];
  const bool modeChanged =
    state.mInitialized && state.mLastEffectiveMode != effectiveMode;
  state.mLastEffectiveMode = effectiveMode;
  const float clampedDt = std::clamp(dtSeconds, 0.0f, 0.1f);
  const float alpha = (dtSeconds <= 0.0f)
    ? 0.0f
    : (vr.mHeadFollow.mFollowSpeed >= 20.0f
         ? 1.0f
         : 1.0f - std::exp(-vr.mHeadFollow.mFollowSpeed * clampedDt));

  if (
    effectiveMode == VRFollowMode::LeftHand
    || effectiveMode == VRFollowMode::RightHand) {
    const auto& hf = vr.mHeadFollow;
    state.mLastHandOffsetX = hf.mHandOffsetX;
    state.mLastHandOffsetY = hf.mHandOffsetY;
    state.mLastHandOffsetZ = hf.mHandOffsetZ;
    state.mLastHandPitchDeg = hf.mHandPitchDeg;
    state.mLastHandForwardX = hf.mHandForwardX;
    state.mLastHandForwardY = hf.mHandForwardY;
    state.mLastHandForwardZ = hf.mHandForwardZ;

    if (handMountPose) {
      const Vector3 localPos(hf.mHandOffsetX, hf.mHandOffsetY, hf.mHandOffsetZ);
      const Vector3 forwardHint(
        hf.mHandForwardX, hf.mHandForwardY, hf.mHandForwardZ);
      const Quaternion localOrient =
        ComputeWristLocalOrientation(localPos, forwardHint, hf.mHandPitchDeg);
      const auto tiltMatrix = (std::abs(hf.mWristTiltOffset) > 1e-4f)
        ? Matrix::CreateRotationX(DirectX::XMConvertToRadians(hf.mWristTiltOffset))
        : Matrix::Identity;
      const auto handLocalMatrix =
        tiltMatrix
        * Matrix::CreateFromQuaternion(localOrient)
        * Matrix::CreateTranslation(localPos);
      const auto anchor = MakeHandOffsetAnchor(*handMountPose, hf);
      const auto targetMatrix = handLocalMatrix * anchor;
      const Pose targetPose {
        .mPosition = targetMatrix.Translation(),
        .mOrientation = Quaternion::CreateFromRotationMatrix(targetMatrix),
      };
      state.mInitialized = true;
      state.mCurrentPose = targetPose;
      state.mAnchorHmdPosition = hmdPose.mPosition;
      return state.mCurrentPose;
    }
    if (state.mInitialized && !modeChanged) {
      return state.mCurrentPose;
    }
  }

  const auto& hf = vr.mHeadFollow;
  const auto headLocalMatrix =
    Matrix::CreateRotationX(DirectX::XMConvertToRadians(hf.mHeadPitchDeg))
    * Matrix::CreateRotationY(DirectX::XMConvertToRadians(hf.mHeadYawDeg))
    * Matrix::CreateTranslation({
      hf.mHeadOffsetX,
      hf.mHeadOffsetY,
      hf.mHeadOffsetZ,
    });
  const auto anchor = MakeHorizonAlignedAnchor(hmdPose, hf);
  const auto targetMatrix = headLocalMatrix * anchor;
  const Pose targetPose {
    .mPosition = targetMatrix.Translation(),
    .mOrientation = Quaternion::CreateFromRotationMatrix(targetMatrix),
  };

  const bool headOffsetsChanged = state.mInitialized
    && (state.mLastHeadOffsetX != hf.mHeadOffsetX
        || state.mLastHeadOffsetY != hf.mHeadOffsetY
        || state.mLastHeadOffsetZ != hf.mHeadOffsetZ
        || state.mLastHeadPitchDeg != hf.mHeadPitchDeg
        || state.mLastHeadYawDeg != hf.mHeadYawDeg);
  state.mLastHeadOffsetX = hf.mHeadOffsetX;
  state.mLastHeadOffsetY = hf.mHeadOffsetY;
  state.mLastHeadOffsetZ = hf.mHeadOffsetZ;
  state.mLastHeadPitchDeg = hf.mHeadPitchDeg;
  state.mLastHeadYawDeg = hf.mHeadYawDeg;

  if (
    !state.mInitialized || modeChanged || headOffsetsChanged
    || effectiveMode == VRFollowMode::HeadLocked) {
    state.mInitialized = true;
    state.mCurrentPose = targetPose;
    state.mAnchorHmdPosition = hmdPose.mPosition;
    return targetPose;
  }

  if (effectiveMode == VRFollowMode::HeadFollowSmooth) {
    state.mCurrentPose.mPosition = Vector3::Lerp(
      state.mCurrentPose.mPosition, targetPose.mPosition, alpha);
    state.mCurrentPose.mOrientation = Quaternion::Slerp(
      state.mCurrentPose.mOrientation, targetPose.mOrientation, alpha);
    state.mAnchorHmdPosition = hmdPose.mPosition;
    return state.mCurrentPose;
  }

  const float angErrDeg = QuaternionAngleDeg(
    state.mCurrentPose.mOrientation, targetPose.mOrientation);
  const float hmdMoveM = Vector3::Distance(
    hmdPose.mPosition, state.mAnchorHmdPosition);

  const bool outside = angErrDeg > vr.mHeadFollow.mDeadzoneAngleDeg
    || hmdMoveM > vr.mHeadFollow.mDeadzoneDistanceM;
  const bool keepFollowing = state.mEngaged && (
    angErrDeg > vr.mHeadFollow.mDeadzoneAngleDeg * 0.2f
    || hmdMoveM > vr.mHeadFollow.mDeadzoneDistanceM * 0.2f);

  state.mEngaged = outside || keepFollowing;
  if (state.mEngaged) {
    state.mCurrentPose.mPosition = Vector3::Lerp(
      state.mCurrentPose.mPosition, targetPose.mPosition, alpha);
    state.mCurrentPose.mOrientation = Quaternion::Slerp(
      state.mCurrentPose.mOrientation, targetPose.mOrientation, alpha);
    state.mAnchorHmdPosition = Vector3::Lerp(
      state.mAnchorHmdPosition, hmdPose.mPosition, alpha);
  }
  return state.mCurrentPose;
}

Vector2 VRKneeboard::GetKneeboardSize(
  const SHM::Config& config,
  const SHM::LayerConfig& layer,
  bool isLookingAtKneeboard) {
  const auto sizes = this->GetSizes(config.mVR, layer);

  return config.mVR.mForceZoom
      || (isLookingAtKneeboard && layer.mVR.mEnableGazeZoom)
    ? sizes.mZoomedSize
    : sizes.mNormalSize;
}

VRKneeboard::Sizes VRKneeboard::GetSizes(
  const VRRenderSettings&,
  const SHM::LayerConfig& layer) const {
  const auto& physicalSize = layer.mVR.mPhysicalSize;
  const auto virtualWidth = physicalSize.mWidth;
  const auto virtualHeight = physicalSize.mHeight;

  return {
    .mNormalSize = {virtualWidth, virtualHeight},
    .mZoomedSize =
      {virtualWidth * layer.mVR.mZoomScale,
       virtualHeight * layer.mVR.mZoomScale},
  };
}

void VRKneeboard::MaybeRecenter(
  const VRRenderSettings& vr,
  const Pose& hmdPose) {
  if (vr.mRecenterCount == mRecenterCount) {
    return;
  }
  this->Recenter(vr, hmdPose);
}

void VRKneeboard::Recenter(const VRRenderSettings& vr, const Pose& hmdPose) {
  auto pos = hmdPose.mPosition;
  mEyeHeight = {pos.y};
  pos.y = 0;

  // We're only going to respect ry (yaw) as we want the new
  // center to remain gravity-aligned

  auto quat = hmdPose.mOrientation;

  // clang-format off
    mRecenter =
      Matrix::CreateRotationY(quat.ToEuler().y)
      * Matrix::CreateTranslation({pos.x, pos.y, pos.z});
  // clang-format on

  mRecenterCount = vr.mRecenterCount;
}

std::vector<VRKneeboard::Layer> VRKneeboard::GetLayers(
  const SHM::Config& config,
  const std::span<const SHM::LayerConfig>& layers,
  const Pose& hmdPose,
  const float dtSeconds,
  const std::optional<Pose>& handMountPose) {
  const std::array<std::optional<Pose>, 2> handPoses {
    handMountPose, handMountPose};
  return this->GetLayers(config, layers, hmdPose, dtSeconds, handPoses);
}

std::vector<VRKneeboard::Layer> VRKneeboard::GetLayers(
  const SHM::Config& config,
  const std::span<const SHM::LayerConfig>& layers,
  const Pose& hmdPose,
  const float dtSeconds,
  const std::array<std::optional<Pose>, 2>& handMountPoses) {
  if (!mEyeHeight) {
    mEyeHeight = {hmdPose.mPosition.y};
  }

  auto effectiveConfig = config;
  effectiveConfig.mVR = this->GetEffectiveVRSettings(config.mVR);
  const auto now = std::chrono::steady_clock::now();
  const bool dragging =
    mManipulation.mDragMode != ManipulationState::DragMode::None;

  std::vector<Layer> ret;
  ret.reserve(layers.size());
  size_t visibleLayerIndex = 0;
  for (const auto& layerConfig : layers) {
    if (!layerConfig.mVREnabled) {
      continue;
    }
    const bool isPrimaryVisible = (visibleLayerIndex == 0);
    ++visibleLayerIndex;

    const auto* lState = this->FindLayerState(layerConfig.mLayerID);
    const bool layerDragging =
      dragging && mManipulation.mTargetLayerID == layerConfig.mLayerID;
    const bool layerRecent = lState
      && ((now - lState->mLastChangeTime) < std::chrono::milliseconds(450));

    auto workingConfig = layerConfig;
    if (lState && (layerDragging || layerRecent)) {
      if (lState->mHasOptimisticPose) {
        workingConfig.mVR.mPose = lState->mOptimisticWorldPose;
      }
      if (
        lState->mHasOptimisticSize
        && lState->mOptimisticSize.x > 0.05f
        && lState->mOptimisticSize.y > 0.05f) {
        workingConfig.mVR.mPhysicalSize = {
          lState->mOptimisticSize.x,
          lState->mOptimisticSize.y,
        };
      }
    }

    auto layerEffectiveConfig = config;
    layerEffectiveConfig.mVR = this->GetEffectiveLayerVRSettings(
      config.mVR, layerConfig.mLayerID, isPrimaryVisible);
    const auto layerEffMode =
      GetEffectiveFollowMode(layerEffectiveConfig.mVR.mHeadFollow);
    const std::optional<Pose> layerHandPose =
      (layerEffMode == VRFollowMode::LeftHand)
        ? handMountPoses[0]
        : ((layerEffMode == VRFollowMode::RightHand) ? handMountPoses[1]
                                                     : std::nullopt);

    auto params = GetRenderParameters(
      layerEffectiveConfig,
      workingConfig,
      hmdPose,
      dtSeconds,
      layerHandPose);
    ret.push_back(Layer {&layerConfig, params});
  }

  if (!effectiveConfig.mVR.mEnableGazeInputFocus) {
    return ret;
  }
  const auto activeLayerID = effectiveConfig.mGlobalInputLayerID;

  const auto activeIt =
    std::ranges::find(ret, activeLayerID, [](const Layer& layer) {
      return layer.mLayerConfig->mLayerID;
    });
  if (
    (activeIt != ret.end())
    && (activeIt->mRenderParameters.mIsLookingAtKneeboard)) {
    return ret;
  }

  for (const auto& [layerConfig, renderParams] :
       std::ranges::reverse_view(ret)) {
    if (
      renderParams.mIsLookingAtKneeboard
      && layerConfig->mLayerID != activeLayerID) {
      SHM::ActiveConsumers::SetActiveInGameViewID(layerConfig->mLayerID);
      return ret;
    }
  }

  return ret;
}

VRKneeboard::RenderParameters VRKneeboard::GetRenderParameters(
  const SHM::Config& config,
  const SHM::LayerConfig& layer,
  const Pose& hmdPose,
  const float dtSeconds,
  const std::optional<Pose>& handMountPose) {
  const auto basePose = this->GetKneeboardPose(
    config.mVR, layer, hmdPose, dtSeconds, handMountPose);
  const bool isHandPinned =
    IsHandPinnedMode(GetEffectiveFollowMode(config.mVR.mHeadFollow));
  const auto isLookingAtKneeboard =
    this->IsLookingAtKneeboard(config, layer, hmdPose, basePose);
  const auto kneeboardSize =
    this->GetKneeboardSize(config, layer, isLookingAtKneeboard);
  const auto kneeboardPose = isHandPinned
    ? ApplyHandBottomEdgeAnchor(basePose, kneeboardSize.y)
    : basePose;

  return {
    .mKneeboardPose = kneeboardPose,
    .mKneeboardSize = kneeboardSize,
    .mKneeboardOpacity = isLookingAtKneeboard ? layer.mVR.mOpacity.mGaze
                                              : layer.mVR.mOpacity.mNormal,
    .mIsLookingAtKneeboard = isLookingAtKneeboard,
  };
}

bool VRKneeboard::IsLookingAtKneeboard(
  const SHM::Config& config,
  const SHM::LayerConfig& layer,
  const Pose& hmdPose,
  const Pose& kneeboardPose) {
  auto& isLookingAtKneeboard = mIsLookingAtKneeboard[layer.mLayerID];

  if (
    layer.mVR.mGazeTargetScale.mHorizontal < 0.1
    || layer.mVR.mGazeTargetScale.mVertical < 0.1) {
    return false;
  }

  const auto sizes = this->GetSizes(config.mVR, layer);
  const bool useZoomedBounds =
    config.mVR.mForceZoom
    || (isLookingAtKneeboard && layer.mVR.mEnableGazeZoom);
  auto currentSize = useZoomedBounds ? sizes.mZoomedSize : sizes.mNormalSize;
  const bool isHandPinned =
    IsHandPinnedMode(GetEffectiveFollowMode(config.mVR.mHeadFollow));
  const Pose hitTestPose = isHandPinned
    ? ApplyHandBottomEdgeAnchor(kneeboardPose, currentSize.y)
    : kneeboardPose;

  currentSize.x *= layer.mVR.mGazeTargetScale.mHorizontal;
  currentSize.y *= layer.mVR.mGazeTargetScale.mVertical;

  isLookingAtKneeboard = RayIntersectsRect(
    hmdPose.mPosition,
    hmdPose.mOrientation,
    hitTestPose.mPosition,
    hitTestPose.mOrientation,
    currentSize);

  return isLookingAtKneeboard;
}

}// namespace VDXROVR
