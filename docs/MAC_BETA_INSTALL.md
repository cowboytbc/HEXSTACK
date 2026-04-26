# HEXSTACK macOS beta install guide

This macOS package is an **unsigned beta build**.

That means macOS may warn the user before opening the app or loading the plugin.

## What to install

The package contains:

- `HEXSTACK.vst3`
- `HEXSTACK.component`
- `HEXSTACK.app`

## Where to put the plugin files

### VST3

Copy `HEXSTACK.vst3` to:

`~/Library/Audio/Plug-Ins/VST3/`

### Audio Unit

Copy `HEXSTACK.component` to:

`~/Library/Audio/Plug-Ins/Components/`

## If macOS blocks the files

### Try this first

1. Right-click the file or app.
2. Click **Open**.
3. If macOS warns you, click **Open** again.

### If the plugin is still blocked

1. Open **System Settings**.
2. Go to **Privacy & Security**.
3. Scroll down until you see a message about HEXSTACK being blocked.
4. Click **Open Anyway**.

## If the DAW still does not see the plugin

Open Terminal and run these commands one at a time:

```text
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/HEXSTACK.vst3
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/HEXSTACK.component
```

Then reopen the DAW and rescan plugins.

## If the standalone app is blocked

Open Terminal and run:

```text
xattr -dr com.apple.quarantine /path/to/HEXSTACK.app
```

Replace `/path/to/HEXSTACK.app` with the actual app location.

## Important note

This beta build is for testing. A future signed/notarized release should require less manual approval from macOS.