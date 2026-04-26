# HEXSTACK Windows install guide

This package includes:

- `Install-HEXSTACK-Windows.ps1`
- `HEXSTACK.vst3`
- `HEXSTACK.exe`

## Easiest way

Run:

`Install-HEXSTACK-Windows.ps1`

The installer helper lets the user choose:

- VST3 plugin or skip it
- Standalone app or skip it
- default install folders or custom folders

## Default Windows install folders used by the helper

- VST3: `%LOCALAPPDATA%\Programs\Common\VST3\`
- Standalone app: `%LOCALAPPDATA%\Programs\HEXSTACK\`

These defaults avoid admin prompts for most users.

## If PowerShell blocks the script

Right-click the script and choose **Run with PowerShell**.

If Windows still blocks it, open PowerShell and run:

```text
Set-ExecutionPolicy -Scope Process Bypass
```

Then run the installer script again.