---
parent: APIs
---

# Registry Values

## `HKEY_CURRENT_USER\SOFTWARE\NikitaShulepov\VDXROVR`

### `InstallationBinPath`

`REG_SZ` - *read-only, v1.8.4 and above*

On startup, VDXROVR writes the path of the folder containing `VDXROVRApp.exe`, along with the API DLLs. Remote controls can be founded by checking `..\utilities\`

This will usually contain `C:\Program Files\VDXROVR\bin`

VDXROVR only writes this value, it does not read it.

### `SettingsPath`

`REG_SZ` - *read-only, v1.8.5 and above*

On startup, VDXROVR writes the path of its' settings folder.

This will usually contain `C:\Users\YOUR USERNAME\Saved Games\VDXROVR`.

VDXROVR only writes this value, it does not read it. **It can not be used to change where VDXROVR looks for settings**.

While you're welcome to read VDXROVR's settings files, DO NOT WRITE SOFTWARE THAT MODIFIES VDXROVR'S CONFIGURATION FILES - YOU **WILL** BREAK USER'S INSTALLATIONS WHEN THEY UPGRADE. If there is not an existing API for the change you want to make, file a feature request GitHub issue.