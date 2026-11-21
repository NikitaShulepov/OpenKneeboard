// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <VDXROVR/CreateTabActions.hpp>
#include <VDXROVR/CursorClickableRegions.hpp>
#include <VDXROVR/CursorEvent.hpp>
#include <VDXROVR/CursorRenderer.hpp>
#include <VDXROVR/D3D11.hpp>
#include <VDXROVR/GetSystemColor.hpp>
#include <VDXROVR/ITab.hpp>
#include <VDXROVR/InterprocessRenderer.hpp>
#include <VDXROVR/KneeboardState.hpp>
#include <VDXROVR/KneeboardView.hpp>
#include <VDXROVR/SpriteAtlas.hpp>
#include <VDXROVR/TabView.hpp>
#include <VDXROVR/ToolbarAction.hpp>

#include <VDXROVR/dprint.hpp>
#include <VDXROVR/hresult.hpp>
#include <VDXROVR/scope_exit.hpp>
#include <VDXROVR/tracing.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <mutex>
#include <ranges>

#include <DirectXColors.h>
#include <d3d11_4.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wincodec.h>

namespace VDXROVR {

void InterprocessRenderer::SubmitFrame(
  const std::vector<SHM::LayerConfig>& shmLayers,
  uint64_t /*inputLayerID*/) noexcept {
  if (!mSHM) {
    return;
  }

  VDXROVR_TraceLoggingScopedActivity(
    activity, "InterprocessRenderer::SubmitFrame()");

  auto ctx = mDXR->mD3D11ImmediateContext.get();
  const D3D11_BOX srcBox {
    0,
    0,
    0,
    static_cast<UINT>(mCanvasSize.mWidth),
    static_cast<UINT>(mCanvasSize.mHeight),
    1,
  };

  auto srcTexture = mCanvas->d3d().texture();

  TraceLoggingWriteTagged(activity, "AcquireSHMLock/start");
  const std::unique_lock shmLock(mSHM);
  TraceLoggingWriteTagged(activity, "AcquireSHMLock/stop");

  auto ipcTextureInfo = mSHM.BeginFrame();
  auto destResources =
    this->GetIPCTextureResources(ipcTextureInfo.mTextureIndex, mCanvasSize);

  auto fence = destResources->mFence.get();
  {
    VDXROVR_TraceLoggingScope(
      "CopyFromCanvas",
      TraceLoggingValue(ipcTextureInfo.mTextureIndex, "TextureIndex"),
      TraceLoggingValue(ipcTextureInfo.mFenceOut, "FenceOut"));
    {
      VDXROVR_TraceLoggingScope("CopyFromCanvas/CopySubresourceRegion");
      ctx->CopySubresourceRegion(
        destResources->mTexture.get(), 0, 0, 0, 0, srcTexture, 0, &srcBox);
    }
    {
      VDXROVR_TraceLoggingScope("CopyFromCanvas/FenceOut");
      check_hresult(ctx->Signal(fence, ipcTextureInfo.mFenceOut));
    }
  }

  // There is no active view if every view is hidden in VR
  const auto activeView = mKneeboard->GetActiveInGameView();
  SHM::Config config {
    .mGlobalInputLayerID =
      activeView ? activeView->GetRuntimeID().GetTemporaryValue() : 0,
    .mVR = static_cast<const VRRenderSettings&>(mKneeboard->GetVRSettings()),
    .mTextureSize = destResources->mTextureSize,
  };
  const auto tint = mKneeboard->GetUISettings().mTint;
  if (tint.mEnabled) {
    config.mTint = {
      tint.mRed * tint.mBrightness,
      tint.mGreen * tint.mBrightness,
      tint.mBlue * tint.mBrightness,
      /* alpha = */ 1.0f,
    };
  }

  {
    VDXROVR_TraceLoggingScope("SHMSubmitFrame");
    mSHM.SubmitFrame(
      ipcTextureInfo,
      config,
      shmLayers,
      destResources->mTextureHandle.get(),
      destResources->mFenceHandle.get());
  }
}

uint64_t InterprocessRenderer::GetFrameCountForMetricsOnly() const {
  return mSHM.GetFrameCountForMetricsOnly();
}

void InterprocessRenderer::InitializeCanvas(const PixelSize& size) {
  if (mCanvasSize == size) {
    return;
  }

  VDXROVR_TraceLoggingScope("InterprocessRenderer::InitializeCanvas()");

  if (size.IsEmpty()) [[unlikely]] {
    VDXROVR_BREAK;
    return;
  }

  D3D11_TEXTURE2D_DESC desc {
    .Width = static_cast<UINT>(size.mWidth),
    .Height = static_cast<UINT>(size.mHeight),
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
    .SampleDesc = {1, 0},
    .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
  };

  auto device = mDXR->mD3D11Device.get();

  winrt::com_ptr<ID3D11Texture2D> texture;
  check_hresult(device->CreateTexture2D(&desc, nullptr, texture.put()));
  mCanvas =
    RenderTargetWithMultipleIdentities::Create(mDXR, texture, MaxViewCount);
  mCanvasSize = size;

  // Let's force a clean start on the clients, including resetting the session
  // ID
  {
    const std::unique_lock shmLock(mSHM);
    mSHM.Detach();
  }
}

void InterprocessRenderer::PostUserAction(UserAction action) {
  switch (action) {
    case UserAction::TOGGLE_VISIBILITY:
      mVisible = !mVisible;
      break;
    case UserAction::SHOW:
      mVisible = true;
      break;
    case UserAction::HIDE:
      mVisible = false;
      break;
    default:
      VDXROVR_BREAK;
      return;
  }

  // Force an SHM update, even if we don't have new pixels
  if (!mVisible) {
    mPreviousFrameWasVisible = true;
  }

  mKneeboard->SetRepaintNeeded();
}

InterprocessRenderer::IPCTextureResources*
InterprocessRenderer::GetIPCTextureResources(
  uint8_t textureIndex,
  const PixelSize& size) {
  auto& ret = mIPCSwapchain.at(textureIndex);
  if (ret.mTextureSize == size) [[likely]] {
    return &ret;
  }

  VDXROVR_TraceLoggingScopedActivity(
    activity,
    "InterprocessRenderer::GetIPCTextureResources:",
    TraceLoggingValue(textureIndex, "textureIndex"),
    TraceLoggingValue(size.mWidth, "width"),
    TraceLoggingValue(size.mHeight, "height"));

  ret = {};

  auto device = mDXR->mD3D11Device.get();

  D3D11_TEXTURE2D_DESC textureDesc {
    .Width = static_cast<UINT>(size.mWidth),
    .Height = static_cast<UINT>(size.mHeight),
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
    .SampleDesc = {1, 0},
    .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    .MiscFlags =
      D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED,
  };

  check_hresult(
    device->CreateTexture2D(&textureDesc, nullptr, ret.mTexture.put()));
  // Our IPC textures will be used within SHM::SwapchainLength (3) frames, so
  // evicting them from VRAM to RAM will pretty much always cause problems.
  ret.mTexture->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);

  check_hresult(device->CreateRenderTargetView(
    ret.mTexture.get(), nullptr, ret.mRenderTargetView.put()));
  check_hresult(ret.mTexture.as<IDXGIResource1>()->CreateSharedHandle(
    nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, ret.mTextureHandle.put()));

  TraceLoggingWriteTagged(activity, "Creating new fence");
  check_hresult(device->CreateFence(
    0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(ret.mFence.put())));
  check_hresult(ret.mFence->CreateSharedHandle(
    nullptr, GENERIC_ALL, nullptr, ret.mFenceHandle.put()));

  ret.mViewport = {
    0,
    0,
    static_cast<FLOAT>(size.mWidth),
    static_cast<FLOAT>(size.mHeight),
    0.0f,
    1.0f,
  };
  ret.mTextureSize = size;

  return &ret;
}

std::shared_ptr<InterprocessRenderer> InterprocessRenderer::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard) {
  auto ret = shared_with_final_release(new InterprocessRenderer(dxr));
  ret->Initialize(kneeboard);
  return ret;
}

VDXROVR::fire_and_forget InterprocessRenderer::final_release(
  std::unique_ptr<InterprocessRenderer> it) {
  // Delete in the correct thread
  co_await it->mOwnerThread;
}

std::mutex InterprocessRenderer::sSingleInstance;

InterprocessRenderer::InterprocessRenderer(const audited_ptr<DXResources>& dxr)
  : mInstanceLock(sSingleInstance),
    mDXR(dxr),
    mSHM(dxr->mAdapterLUID) {
  dprint(__FUNCTION__);
}

void InterprocessRenderer::Initialize(KneeboardState* kneeboard) {
  mKneeboard = kneeboard;
}

InterprocessRenderer::~InterprocessRenderer() {
  dprint(__FUNCTION__);
  this->RemoveAllEventListeners();
  {
    // SHM::Writer's destructor will do this, but let's make sure to
    // tear it down before the vtable and other members go - especially
    // the D3D resources
    const std::unique_lock shmLock(mSHM);
    mSHM.Detach();
  }
  {
    const std::unique_lock d2dlock(*mDXR);
    auto ctx = mDXR->mD2DDeviceContext.get();
    ctx->Flush();
    // De-allocate D3D resources while we have the lock
    mIPCSwapchain = {};
    mCanvas = {};
  }
}

task<SHM::LayerConfig> InterprocessRenderer::RenderLayer(
  const ViewRenderInfo& layer,
  const PixelRect& bounds) noexcept {
  VDXROVR_TraceLoggingScope("InterprocessRenderer::RenderLayer");
  const auto view = layer.mView.get();

  SHM::LayerConfig ret {};
  ret.mLayerID = view->GetRuntimeID().GetTemporaryValue();

  if (layer.mVR) {
    ret.mVREnabled = true;
    ret.mVR = *layer.mVR;
    ret.mVR.mLocationOnTexture.mOffset.mX += bounds.mOffset.mX;
    ret.mVR.mLocationOnTexture.mOffset.mY += bounds.mOffset.mY;
  }

  const PixelRect layerRect {bounds.mOffset, layer.mFullSize};
  co_await view->RenderWithChrome(
    mCanvas.get(),
    layerRect,
    layer.mIsActiveForInput);

  co_return ret;
}

task<void> InterprocessRenderer::RenderNow() noexcept {
  if (mRendering.test_and_set()) {
    dprint("Two renders in the same instance");
    VDXROVR_BREAK;
    co_return;
  }

  const scope_exit markDone([this]() { mRendering.clear(); });

  VDXROVR_TraceLoggingScopedActivity(
    activity, "InterprocessRenderer::RenderNow()");

  const auto renderInfos = mKneeboard->GetViewRenderInfo();
  const auto layerCount = renderInfos.size();

  // layerCount == 0 'should' be impossible as it's not meant to be possible to
  // disable the non-VR view for view 1, however a bug in v1.10.0 and v1.10.2
  // could lead to view 1 being fully disabled
  if (layerCount == 0 || !mVisible) {
    if (layerCount == 0) {
      TraceLoggingWriteTagged(activity, "NoLayers");
    } else {
      TraceLoggingWriteTagged(activity, "Invisible");
    }
    if (mSHM && mPreviousFrameWasVisible) {
      std::unique_lock lock(mSHM);
      mSHM.SubmitEmptyFrame();
    }
    mPreviousFrameWasVisible = false;
    co_return;
  }
  mPreviousFrameWasVisible = true;

  const auto viewSizes =
    renderInfos | std::views::transform(&ViewRenderInfo::mFullSize);
  if (!std::ranges::equal(mViewSizes, viewSizes)) {
    mViewSizes = {viewSizes.begin(), viewSizes.end()};
    mSpriteAtlas.emplace(mViewSizes);
  }
  const auto& sprites = mSpriteAtlas.value();
  auto canvasSize = sprites.GetAtlasSize();
  const uint32_t gizmoY = canvasSize.mHeight;
  canvasSize.mWidth = (std::max)(canvasSize.mWidth, 768u);
  canvasSize.mHeight += 104u;

  TraceLoggingWriteTagged(activity, "AcquireDXLock/start");
  const std::unique_lock dxlock(*mDXR);
  TraceLoggingWriteTagged(activity, "AcquireDXLock/stop");
  this->InitializeCanvas(canvasSize);
  mDXR->mD3D11ImmediateContext->ClearRenderTargetView(
    mCanvas->d3d().rtv(), DirectX::Colors::Transparent);

  std::vector<SHM::LayerConfig> shmLayers;
  shmLayers.reserve(layerCount);
  uint64_t inputLayerID = 0;

  for (uint8_t i = 0; i < layerCount; ++i) {
    const auto bounds = sprites.GetRect(i);
    const auto& renderInfo = renderInfos.at(i);
    if (renderInfo.mIsActiveForInput) {
      inputLayerID = renderInfo.mView->GetRuntimeID().GetTemporaryValue();
    }

    mCanvas->SetActiveIdentity(i);

    shmLayers.push_back(co_await this->RenderLayer(renderInfo, bounds));
  }

  {
    auto d2d = mCanvas->d2d();
    const float gy = static_cast<float>(gizmoY);
    winrt::com_ptr<ID2D1SolidColorBrush> haloBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> cyanBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> whiteBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> darkRingBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> amberBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> goldBrush;
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.22f, 0.74f, 0.97f, 0.14f), haloBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.49f, 0.83f, 0.99f, 0.36f), cyanBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.84f, 0.93f, 0.99f, 0.50f), whiteBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.04f, 0.07f, 0.12f, 0.45f), darkRingBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.96078f, 0.61961f, 0.04314f, 0.95f), amberBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.99216f, 0.87843f, 0.27843f, 1.0f), goldBrush.put()));

    // [0..32]: Laser beam vertical sprite (subtle halo -> cyan -> soft core)
    d2d->FillRectangle(
      D2D1::RectF(8.0f, gy + 1.0f, 24.0f, gy + 31.0f), haloBrush.get());
    d2d->FillRectangle(
      D2D1::RectF(12.0f, gy + 1.0f, 20.0f, gy + 31.0f), cyanBrush.get());
    d2d->FillRectangle(
      D2D1::RectF(14.5f, gy + 1.0f, 17.5f, gy + 31.0f), whiteBrush.get());

    // [32..64]: Hit reticle dot
    const D2D1_POINT_2F c {48.0f, gy + 16.0f};
    d2d->FillEllipse(D2D1::Ellipse(c, 11.0f, 11.0f), darkRingBrush.get());
    d2d->FillEllipse(D2D1::Ellipse(c, 7.5f, 7.5f), cyanBrush.get());
    d2d->FillEllipse(D2D1::Ellipse(c, 3.2f, 3.2f), whiteBrush.get());

    // [64..96]: Solid amber swatch for runtime Edit Mode border bars
    d2d->FillRectangle(
      D2D1::RectF(64.0f, gy, 96.0f, gy + 32.0f), amberBrush.get());

    // [96..128]: Solid bright gold swatch for hovered/active corner handles
    d2d->FillRectangle(
      D2D1::RectF(96.0f, gy, 128.0f, gy + 32.0f), goldBrush.get());

    // [128..256]: Pure borderless chevron arrow (no box, no background)
    {
      winrt::com_ptr<ID2D1Factory> d2dFactory;
      d2d->GetFactory(d2dFactory.put());
      winrt::com_ptr<ID2D1StrokeStyle> roundStroke;
      if (d2dFactory) {
        const D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties(
          D2D1_CAP_STYLE_ROUND,
          D2D1_CAP_STYLE_ROUND,
          D2D1_CAP_STYLE_ROUND,
          D2D1_LINE_JOIN_ROUND);
        d2dFactory->CreateStrokeStyle(&props, nullptr, 0, roundStroke.put());
      }
      winrt::com_ptr<ID2D1SolidColorBrush> chevShadowBrush;
      winrt::com_ptr<ID2D1SolidColorBrush> chevBrush;
      check_hresult(d2d->CreateSolidColorBrush(
        D2D1::ColorF(0.02f, 0.04f, 0.08f, 0.72f), chevShadowBrush.put()));
      check_hresult(d2d->CreateSolidColorBrush(
        D2D1::ColorF(0.95f, 0.97f, 0.99f, 0.98f), chevBrush.put()));
      const D2D1_POINT_2F pL {156.0f, gy + 11.0f};
      const D2D1_POINT_2F pM {192.0f, gy + 22.0f};
      const D2D1_POINT_2F pR {228.0f, gy + 11.0f};
      d2d->DrawLine(pL, pM, chevShadowBrush.get(), 6.5f, roundStroke.get());
      d2d->DrawLine(pM, pR, chevShadowBrush.get(), 6.5f, roundStroke.get());
      d2d->DrawLine(pL, pM, chevBrush.get(), 4.0f, roundStroke.get());
      d2d->DrawLine(pM, pR, chevBrush.get(), 4.0f, roundStroke.get());
    }

    // [0..768] x [gy+32..gy+104]: In-VR floating 3D Control Bar (mode + distance + multi-tab spawn/close)
    const float dy = gy + 32.0f;
    constexpr float dw = 768.0f;
    constexpr float dh = 72.0f;
    const auto vrSettings = mKneeboard->GetVRSettings();
    const auto& hf = vrSettings.mHeadFollow;
    const auto effectiveMode = GetEffectiveFollowMode(hf);
    const bool isHandMode =
      effectiveMode == VRFollowMode::LeftHand
      || effectiveMode == VRFollowMode::RightHand;
    const bool isHeadMode =
      effectiveMode == VRFollowMode::HeadFollowLazy
      || effectiveMode == VRFollowMode::HeadFollowSmooth
      || effectiveMode == VRFollowMode::HeadLocked;

    winrt::com_ptr<ID2D1SolidColorBrush> dockBgBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> dockBorderBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> btnInactiveBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> btnActiveBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> btnStrokeBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> textLightBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> textDarkBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> textMutedBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> readoutAccentBrush;
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.035f, 0.055f, 0.095f, 0.94f), dockBgBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      vrSettings.mEditModeEnabled
        ? D2D1::ColorF(0.984f, 0.749f, 0.141f, 0.82f)
        : D2D1::ColorF(0.580f, 0.639f, 0.722f, 0.38f),
      dockBorderBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.118f, 0.161f, 0.231f, 0.84f), btnInactiveBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.851f, 0.467f, 0.024f, 0.94f), btnActiveBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.580f, 0.639f, 0.722f, 0.30f), btnStrokeBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.886f, 0.910f, 0.941f, 1.0f), textLightBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.043f, 0.067f, 0.125f, 1.0f), textDarkBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.580f, 0.639f, 0.722f, 1.0f), textMutedBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      isHandMode
        ? D2D1::ColorF(0.984f, 0.749f, 0.141f, 1.0f)
        : D2D1::ColorF(0.220f, 0.741f, 0.973f, 1.0f),
      readoutAccentBrush.put()));

    d2d->FillRoundedRectangle(
      D2D1::RoundedRect(D2D1::RectF(2.0f, dy + 2.0f, dw - 2.0f, dy + dh - 2.0f), 14.0f, 14.0f),
      dockBgBrush.get());
    d2d->DrawRoundedRectangle(
      D2D1::RoundedRect(D2D1::RectF(2.0f, dy + 2.0f, dw - 2.0f, dy + dh - 2.0f), 14.0f, 14.0f),
      dockBorderBrush.get(),
      2.0f);

    const float divX = dw * 0.598f;
    d2d->DrawLine(
      D2D1::Point2F(divX, dy + 14.0f),
      D2D1::Point2F(divX, dy + dh - 14.0f),
      btnStrokeBrush.get(),
      1.5f);
    const float divX2 = dw * 0.818f;
    d2d->DrawLine(
      D2D1::Point2F(divX2, dy + 14.0f),
      D2D1::Point2F(divX2, dy + dh - 14.0f),
      btnStrokeBrush.get(),
      1.5f);

    winrt::com_ptr<IDWriteTextFormat> btnFormat;
    winrt::com_ptr<IDWriteTextFormat> symFormat;
    winrt::com_ptr<IDWriteTextFormat> readoutFormat;
    winrt::com_ptr<IDWriteTextFormat> subFormat;
    if (mDXR && mDXR->mDWriteFactory) {
      check_hresult(mDXR->mDWriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        14.5f,
        L"",
        btnFormat.put()));
      check_hresult(mDXR->mDWriteFactory->CreateTextFormat(
        L"Consolas",
        nullptr,
        DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        24.0f,
        L"",
        symFormat.put()));
      check_hresult(mDXR->mDWriteFactory->CreateTextFormat(
        L"Consolas",
        nullptr,
        DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        15.5f,
        L"",
        readoutFormat.put()));
      check_hresult(mDXR->mDWriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        10.0f,
        L"",
        subFormat.put()));
    }

    const auto drawCenteredText = [&](
      const std::wstring& text,
      IDWriteTextFormat* fmt,
      const D2D1_RECT_F& rect,
      ID2D1SolidColorBrush* brush) {
      if (!fmt || !mDXR || !mDXR->mDWriteFactory) {
        return;
      }
      winrt::com_ptr<IDWriteTextLayout> layout;
      if (FAILED(mDXR->mDWriteFactory->CreateTextLayout(
            text.c_str(),
            static_cast<UINT32>(text.size()),
            fmt,
            (std::max)(1.0f, rect.right - rect.left),
            (std::max)(1.0f, rect.bottom - rect.top),
            layout.put()))) {
        return;
      }
      layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
      layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
      d2d->DrawTextLayout(D2D1::Point2F(rect.left, rect.top), layout.get(), brush);
    };

    const std::wstring headLabel = !isHeadMode
      ? L"Head"
      : (effectiveMode == VRFollowMode::HeadFollowSmooth
           ? L"Head \u00B7 Smooth"
           : (effectiveMode == VRFollowMode::HeadLocked ? L"Head \u00B7 HUD"
                                                        : L"Head \u00B7 Cone"));
    winrt::com_ptr<ID2D1SolidColorBrush> spawnBgBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> spawnStrokeBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> spawnTextBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> closeBgBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> closeStrokeBrush;
    winrt::com_ptr<ID2D1SolidColorBrush> closeTextBrush;
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.063f, 0.290f, 0.220f, 0.92f), spawnBgBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.204f, 0.827f, 0.600f, 0.68f), spawnStrokeBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.655f, 0.953f, 0.816f, 1.0f), spawnTextBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.365f, 0.082f, 0.133f, 0.94f), closeBgBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.984f, 0.443f, 0.522f, 0.72f), closeStrokeBrush.put()));
    check_hresult(d2d->CreateSolidColorBrush(
      D2D1::ColorF(0.996f, 0.804f, 0.827f, 1.0f), closeTextBrush.put()));

    enum class DockTone : uint8_t { Default, Active, Spawn, Close };
    struct DockBtnSpec {
      float uMin;
      float uMax;
      DockTone tone;
      bool isSym;
      std::wstring label;
    };
    const bool canCloseOverlay = shmLayers.size() > 1;
    const bool highlightSharedModes = shmLayers.size() <= 1;
    const std::array<DockBtnSpec, 8> buttons {{
      {0.012f, 0.132f,
       (highlightSharedModes && effectiveMode == VRFollowMode::WorldLocked)
         ? DockTone::Active
         : DockTone::Default,
       false,
       (highlightSharedModes && effectiveMode == VRFollowMode::WorldLocked)
         ? L"\u25CF World"
         : L"World"},
      {0.140f, 0.305f,
       (highlightSharedModes && isHeadMode) ? DockTone::Active : DockTone::Default,
       false,
       highlightSharedModes ? headLabel : L"Head"},
      {0.313f, 0.448f,
       (highlightSharedModes && effectiveMode == VRFollowMode::LeftHand)
         ? DockTone::Active
         : DockTone::Default,
       false,
       (highlightSharedModes && effectiveMode == VRFollowMode::LeftHand)
         ? L"\u00D7 L Hand"
         : L"L Hand"},
      {0.456f, 0.591f,
       (highlightSharedModes && effectiveMode == VRFollowMode::RightHand)
         ? DockTone::Active
         : DockTone::Default,
       false,
       (highlightSharedModes && effectiveMode == VRFollowMode::RightHand)
         ? L"\u00D7 R Hand"
         : L"R Hand"},
      {0.606f, 0.666f, DockTone::Default, true, L"-"},
      {0.752f, 0.812f, DockTone::Default, true, L"+"},
      {0.824f, canCloseOverlay ? 0.916f : 0.988f, DockTone::Spawn, false, L"+ Tab"},
      {0.924f, 0.988f, DockTone::Close, true, L"\u00D7"},
    }};

    for (size_t bi = 0; bi < buttons.size(); ++bi) {
      const auto& b = buttons[bi];
      if (!canCloseOverlay && b.tone == DockTone::Close) {
        continue;
      }
      const D2D1_RECT_F r = D2D1::RectF(
        b.uMin * dw, dy + 10.0f, b.uMax * dw, dy + dh - 10.0f);
      ID2D1SolidColorBrush* bg = btnInactiveBrush.get();
      ID2D1SolidColorBrush* stroke = btnStrokeBrush.get();
      ID2D1SolidColorBrush* fg = textLightBrush.get();
      float strokeW = 1.25f;
      if (b.tone == DockTone::Active) {
        bg = btnActiveBrush.get();
        stroke = goldBrush.get();
        fg = textDarkBrush.get();
        strokeW = 2.0f;
      } else if (b.tone == DockTone::Spawn) {
        bg = spawnBgBrush.get();
        stroke = spawnStrokeBrush.get();
        fg = spawnTextBrush.get();
        strokeW = 1.5f;
      } else if (b.tone == DockTone::Close) {
        bg = closeBgBrush.get();
        stroke = closeStrokeBrush.get();
        fg = closeTextBrush.get();
        strokeW = 1.5f;
      }
      d2d->FillRoundedRectangle(D2D1::RoundedRect(r, 9.0f, 9.0f), bg);
      d2d->DrawRoundedRectangle(D2D1::RoundedRect(r, 9.0f, 9.0f), stroke, strokeW);
      drawCenteredText(
        b.label, b.isSym ? symFormat.get() : btnFormat.get(), r, fg);
    }

    float distM = 0.55f;
    if (isHandMode) {
      const float handLen = std::hypot(
        hf.mHandOffsetX, std::hypot(hf.mHandOffsetY, hf.mHandOffsetZ));
      distM = std::clamp(handLen > 1e-4f ? handLen : 0.10f, 0.06f, 0.65f);
    } else if (isHeadMode) {
      distM = std::clamp(std::abs(hf.mHeadOffsetZ), 0.20f, 1.35f);
    } else if (!shmLayers.empty()) {
      const auto& p = shmLayers.front().mVR.mPose;
      distM = std::clamp(
        std::hypot(p.mX, std::hypot(p.mEyeY, p.mZ)), 0.20f, 2.0f);
    }
    const int distCm = static_cast<int>(std::lround(distM * 100.0f));
    const std::wstring distStr = std::format(L"{} cm", distCm);
    const D2D1_RECT_F valRect = D2D1::RectF(
      0.668f * dw, dy + 10.0f, 0.750f * dw, dy + 42.0f);
    const D2D1_RECT_F subRect = D2D1::RectF(
      0.668f * dw, dy + 42.0f, 0.750f * dw, dy + 62.0f);
    drawCenteredText(
      distStr, readoutFormat.get(), valRect, readoutAccentBrush.get());
    drawCenteredText(
      isHandMode ? L"HAND" : L"DIST",
      subFormat.get(),
      subRect,
      textMutedBrush.get());
  }
  this->SubmitFrame(shmLayers, inputLayerID);
}

}// namespace VDXROVR
