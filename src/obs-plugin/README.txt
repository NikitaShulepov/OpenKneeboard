VDXROVR VDXR Mirror — Native OBS Studio Plugin (Windows x64)
==================================================================

This plugin adds a unified source:
  "VDXR Mirror (Video + Game Audio)"

Features:
- Captures 16:9 OpenXR eye projection (Left / Right / Side-by-Side) at 720p or 1080p
- Optional temporal smoothing, rotational stabilization, and VR overlays
- Isolated VR game process audio via Windows WASAPI Process Loopback (no Discord/system sounds)

Installation:
1. Recommended: Open VDXROVR -> Settings -> VR -> VDXR Mirror (OBS)
   and click "Install / Open OBS Plugin".
2. Manual: Copy the entire "obs-plugin" folder contents into:
     %APPDATA%\obs-studio\plugins\VDXROVR-OBS-Mirror\
   so that the DLL path is:
     %APPDATA%\obs-studio\plugins\VDXROVR-OBS-Mirror\bin\64bit\VDXROVR-OBS-Mirror.dll
3. Restart OBS Studio and add "VDXR Mirror (Video + Game Audio)" to your scene.
