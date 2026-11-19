---
parent: Troubleshooting
---

# Crashes or Freezes

**Crash**: VDXROVR's window closes by itself, *sometimes* with an error message
**Freeze**: VDXROVR still has a window, but it ignores keyboard, mouse, and other input. Windows *might* say "The application has stopped responding."

These steps **only** apply for VDXROVR crashes; if the game crashes but VDXROVR doesn't, a dump or crash file from the game is useful instead. Either way, please include VDXROVR logs when reporting all issues.

## Reporting crashes or freezes

Please do not report crashes or freezes if:

- you are running VDXROVR as administrator, or "[Administrator]" is in the window title; there are known bugs in the Windows App SDK that can not be worked around until Microsoft fix them.
- you have an XP-Pen tablet driver installed and the crash is when enabling WinTab
- you have Gaomon tablet driver installed and the crash is when enabling WinTab
- you have `C:\Windows\System32\WinTab32.dll` with that specific capitalization of `WinTab` and the crash is when enabling Wintab

If you have an unsupported tablet driver, you might want to [try OpenTabletDriver](https://github.com/NikitaShulepov/VDXROVR) instead.

**Please report crashes or freezes via [GitHub issues](https://github.com/NikitaShulepov/VDXROVR)**, and attach a zip file containing a single relevant (recent) dump file.
- **WARNING**: these files can contain some personal information, such as the name of your computer, your username (e.g. it may contain `C:\Users\fred` which says your username is `fred`) and the contents of your kneeboard
- If the file is too large, use a service like Google Drive, Dropbox, Onedrive or similar. If you use a private Google Drive, invite `https://github.com/NikitaShulepov/VDXROVR/issues`; **I do not read email sent to this address**.
- If you would prefer to share the dump privately, **a GitHub issue is still required**, but you can send a link to the dump file to `vdxrovr-dumps@fred.io.github.nikitashulepov.vdxrovr`; if you send an email, mention it in your GitHub issue, or it will not be seen
- If you are able to reliably reproduce the issue, please include steps with as much detail as you can, and any relevant files
- **WARNING**: If you send an email without also filing a GitHub issue, it is very likely that your email will be automatically deleted before it is seen

### Getting a dump file for a crash

**If you are on v1.9 or above:** send the txt and dmp file from `%LOCALAPPDATA%\VDXROVR\Logs\Crashes` instead of following the instructions below.

If you are able to reproduce the issue or it repeats itself over time:

1. Enable full dumps; this may create files > 200mb in size. Run `%PROGRAMFILES%\VDXROVR\scripts\enable-full-dumps.reg`
2. Make VDXROVR crash or wait for it to crash
3. There should be a .dmp file for VDXROVR in `%LOCALAPPDATA%\CrashDumps`; if there are multiple, sort by date, and use the most recent one

If the files are too large, you can try `%PROGRAMFILES%\VDXROVR\scripts\enable-mini-dumps.reg`: this will create much smaller files, but they are much less likely to be useful for diagnosing the problem. They are however much better than no dump if sharing 200mb+ files is not practical for you.

If the crash happened once and you're unable to repeat it:

- check for a recent .dmp file in `Saved Games\VDXROVR`; make sure the time lines up with the crash, then use that
- if there isn't one there, check `%LOCALAPPDATA%\CrashDumps`; depending on your Windows settings and other software, there may be a dmp there even without running either of the previous `.reg` files

### Getting a dump file for a freeze

1. Open Task Manager from Ctrl+Alt+Delete
2. Find "VDXROVRApp"
3. Right click on it
4. Select "Create dump file" from the right-click menu
5. Task Manager will then show a window telling you where it saved the dump

## Crashes on startup, or no window visible

When you start VDXROVR, you should see a window like this:

![A normal Windows app](../screenshots/config-app.png)

If you do not, VDXROVR is crashing on startup.

### Corrupt settings

Try renaming `Saved Games\VDXROVR` to anything else; this will remove your VDXROVR settings.

If that fixes the issue, please [report the crash](#reporting-crashes-or-freezes), and also attach or send a copy of the original VDXROVR saved games folder

*v1.9 and above*: Settings are now in `%LOCALAPPDATA%\VDXROVR\Settings`, not `Saved Games\VDXROVR`.
