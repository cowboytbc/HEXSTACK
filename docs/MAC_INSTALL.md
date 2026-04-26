# HEXSTACK macOS install guide

This package includes:

- `Install-HEXSTACK-Mac.command`
- `HEXSTACK.vst3`
- `HEXSTACK.component`
- `HEXSTACK.app`

## Easiest way

Double-click:

`Install-HEXSTACK-Mac.command`

The installer helper lets the user choose:

- VST3 plugin or skip it
- Audio Unit plugin or skip it
- Standalone app or skip it
- default install folders or custom folders

## Default Mac plugin folders

- VST3: `~/Library/Audio/Plug-Ins/VST3/`
- Audio Unit: `~/Library/Audio/Plug-Ins/Components/`
- Standalone app: `~/Applications/`

## If macOS blocks the app or plugin

1. Right-click the app or installer helper.
2. Click **Open**.
3. If macOS warns you, click **Open** again.

If macOS still blocks the files:

1. Open **System Settings**
2. Go to **Privacy & Security**
3. Scroll down to the blocked app message
4. Click **Open Anyway**

## If the DAW still does not see the plugin

Open Terminal and run these commands:

```text
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/HEXSTACK.vst3
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/HEXSTACK.component
```

Then reopen the DAW and rescan plugins.