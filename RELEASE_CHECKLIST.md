# HEXSTACK v0.2.0 Release Checklist

## Build
- [ ] Configure project with VS 2022 x64 kit
- [ ] Build `Hexstack_VST3`
- [ ] Build `Hexstack_Standalone`
- [ ] Confirm artefacts in `Hexstack_artefacts`

## Functional Audio
- [ ] Input/Drive/Tone/Output/Mix ranges behave correctly
- [ ] Mic and Blend Mic switching works
- [ ] Mic Dist and Mic Blend interpolate smoothly
- [ ] No obvious phase hollowness in stock mic blend
- [ ] External IR loading works (`wav/aiff/aif/flac`)

## Presets
- [ ] Modern Rhythm
- [ ] Tight Lead
- [ ] Doom Wall
- [ ] Glass Clean
- [ ] Industrial Crunch
- [ ] Session reload restores program and settings

## UI
- [ ] Background image auto-load works (root/ui/assets/ui)
- [ ] Tooltip hints visible on hover
- [ ] Diagnostics overlay toggle (`DBG`) works for internal checks
- [ ] Diagnostics overlay hidden for release screenshots

## Host Validation
- [ ] Loads in primary DAW
- [ ] Multiple instances stable
- [ ] Bypass/unbypass stable
- [ ] Project save/reopen stable

## Release Packaging
- [ ] Version metadata set to `0.2.0`
- [ ] Product name: `HEXSTACK`
- [ ] Bundle ID: `com.infernotones.hexstack`
- [ ] Plugin code: `HxSk`
- [ ] README reflects final behavior
- [ ] GitHub Actions `Build HEXSTACK Plugins` run passes on Windows + macOS
- [ ] GitHub Actions `Release HEXSTACK` run produces Windows + macOS zip bundles
- [ ] If a workflow fails, inspect the failing job logs in the GitHub Actions tab before shipping
- [ ] GitHub release tag and uploaded assets match the intended version
- [ ] Apple signing/notarization secrets verified or consciously skipped for test builds
