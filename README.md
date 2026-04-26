# HEXSTACK (Starter Plugin)

A JUCE-based amplifier plugin starter project with:
- Soft-clipping amp saturation (`tanh` waveshaping)
- Tone control (post-distortion low-pass filter)
- Cabinet convolution stage (IR loader)
- Input / Drive / Tone / **Mic Distance** / **Mic Blend** / Output / Mix controls
- Automatic behind-the-scenes phase alignment for stock mic blending
- Built-in **Hexstack OS Mesa V30 4x12** stock cab with multiple mic flavors:
	- **SM57 CapEdge**
	- **SM57 OffAxis**
	- **R121 OffAxis**
	- **MD421 Edge**
	- **e906 Edge**
	- **C414 Room**
- Third-party IR loading (`.wav`, `.aiff`, `.aif`, `.flac`)
- Built-in factory preset pack (Modern Rhythm, Tight Lead, Doom Wall, Glass Clean, Industrial Crunch)
- Plugin formats: **VST3**, **AU**, and **Standalone**

## Requirements (Windows)

- Visual Studio 2022 (Desktop development with C++)
- CMake 3.22+
- Git (optional, but useful)

> JUCE is fetched automatically by CMake from GitHub.

## Build (Windows)

1. Open this folder in VS Code.
2. Configure and build with CMake (via CMake Tools extension), or use your preferred CMake workflow.
3. Build target: `Hexstack_VST3` (and optionally `Hexstack_Standalone`).

After build, the VST3 plugin is generated in your build output under `Hexstack_artefacts`.

## CI and release automation

- `.github/workflows/build-plugins.yml` runs regular Windows + macOS CI builds.
- `.github/workflows/release.yml` packages Windows and macOS release zips and can publish them to GitHub Releases.
- Manual release path: run **Release HEXSTACK** from the Actions tab and provide a tag like `v0.2.0`.
- Tag-driven release path: push a tag like `v0.2.0` and the workflow will package both platforms automatically.
- macOS signing/notarization is optional; the release workflow still succeeds without Apple secrets, but the resulting macOS bundles will be unsigned and/or unnotarized.
- Release assets now include platform-specific installer helpers plus an `HEXSTACK-all-platforms-<tag>.zip` bundle so users can choose Windows or macOS from one download.

See `docs/RELEASE_PIPELINE.md` for the exact release secrets and workflow behavior.

## Load in your DAW

1. Point your DAW's VST3 scan path to the built `.vst3` location (if not in your global VST3 folder).
2. Rescan plugins.
3. Insert **HEXSTACK** on a guitar/audio track.

## Using cabinet IRs

- The UI uses compact rack-style labels (`IN`, `DRV`, `TONE`, `DIST`, `BLND`, `OUT`, `MIX`).
- Hover controls to see detailed parameter tooltips.
- Use the **Mic** selector to switch between stock mic types.
- Use **Blend Mic** to choose the second mic type.
- Use **Mic Dist** to move the virtual mic from near (`0.0`) to farther (`1.0`).
- Use **Mic Blend** to blend between Mic (0.0) and Blend Mic (1.0).
- The stock mic blend path auto-aligns phase internally (peak alignment + polarity correction) to reduce comb filtering.
- Start with **SM57 CapEdge** for tighter aggression, **R121 OffAxis** for smoother thickness,
  and **C414 Room** for a wider/airier feel.
- Click **Load IR...** to load a third-party impulse response file.
- The currently active IR name is shown in the UI next to the buttons.
- IR selection is saved/restored with your DAW session.

## Internal diagnostics mode

- Use the `DBG` button in the top row to toggle internal diagnostics overlay.
- This shows current program, IR source, mic pair, mic distance/blend, sample rate, and mix.
- Keep diagnostics hidden for final screenshots/release demos.

## Custom still-frame background

- Preferred path/name: `assets/ui/hexstack_bg.png`
- Recommended source size: `2048 x 1024` (or `1920 x 1080` fallback)
- The editor auto-loads this image at runtime and uses a **cover** scale (fills all space, no empty borders).
- The plugin window is resizable and the background scales with it.
- Minimum UI size is constrained to prevent control squishing.
- Auto-discovery supports `.png`, `.jpg`, `.jpeg` in:
	- project root
	- `ui/`
	- `assets/ui/`
- Common names like `hexstack_bg`, `background`, and `bg` are detected automatically.
- If no image is found, HEXSTACK uses its built-in procedural background automatically.

## Sound design tips

- Start with: Input `+3 dB`, Drive `0.55`, Tone `0.45`, Output `-6 dB`, Mix `1.0`
- For tighter low end: lower `Tone`
- For parallel grit: set `Mix` around `0.4 - 0.7`

## Next upgrades you can add

- 2x/4x oversampling to reduce aliasing
- Noise gate and input HPF
- Multiple amp modes (Clean / Crunch / Lead)
- Preset system and A/B compare
