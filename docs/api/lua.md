---
parent: APIs
---

# Lua API

## Overview

The Lua API is able to send information, events, or requests to VDXROVR; it is not able to receive data from VDXROVR. It requires a Lua extension - `VDXROVR_LuaAPI64.dll` - which changes with every version of VDXROVR.

This DLL is installed and kept up to date in:

- `C:\Program Files\VDXROVR\bin\VDXROVR_LuaAPI64.dll`
- `Scripts\Hooks\VDXROVR_LuaAPI64.dll` within your DCS saved games path

If you're using it outside of DCS, or outside of `Scripts\Hooks`, you most likely want to load it directly from program files.

## Game Compatibility

VDXROVR's Lua API is only tested with DCS world; the DLL provided with VDXROVR requires that the game:
- uses Lua 5.1.5
- uses `lua.dll`, not `lua51.dll`

Supporting other games will require manually rebuilding VDXROVR, and changing the version or names of the DLLs in VDXROVR's build system. This is not currently supported. I'm open to pull requests adding other Lua build configurations.

32-bit games have the same requirement, but should load `VDXROVR_LuaAPI32.dll` instead.

## Usage

```lua
--[[
	********************************
	**** 1. Set `package.cpath` ****
	********************************

	Tell Lua where to look for the DLLs; replace `SOME_FOLDER` with
	the path to a folder containing an up-to-date `VDXROVR_LuaAPI64.dll`
--]]
package.cpath = "SOME_DIRECTORY\\?.dll;"..package.cpath
-- For example, in DCS:
package.cpath = lfs.writedir().."\\Scripts\\Hooks\\?.dll;"..package.cpath

--[[
	**************************
	**** 2. Load the DLL *****
	**************************
]]--
local status, VDXROVR = pcall(require, "VDXROVR_LuaAPI64")
if status then
  l("DLL Loaded")
else
  l("Failed: "..err)
  return
end

--[[
	******************************************
	**** 3. Send messages to VDXROVR ***
	******************************************
]]--

VDXROVR.sendRaw(
	"RemoteUserAction",
	"NEXT_TAB")
```

## Messages

See [the messages documentation](messages.md) for information on supported messages.

## Sending JSON from DCS

VDXROVR needs JSON data for some messages; in DCS, use `net.lua2json()`, e.g.:

```lua
-- For JSON in DCS LUA:
function VDXROVR.sendJSON(name, value)
  VDXROVR.sendRaw(
	""..name,
	net.lua2json(value))
end

-- For example:
VDXROVR.sendJSON(
	"SetTabByName",
	{
		Name = "Some Tab Title",
		Kneeboard = 2,
		PageNumber = 123,
	})
```
## Examples

- [VDXROVR's DCS extension](https://github.com/NikitaShulepov/VDXROVR/blob/master/src/dcs-hook/VDXROVRDCSExt.lua)
