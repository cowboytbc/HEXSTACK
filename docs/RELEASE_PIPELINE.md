# HEXSTACK release pipeline

This repo now has two GitHub Actions workflows:

- `Build HEXSTACK Plugins` — regular CI build for Windows and macOS
- `Release HEXSTACK` — packages Windows + macOS release zips and can publish a GitHub Release

## Quick start

### Manual release

1. Open **Actions** in GitHub.
2. Run **Release HEXSTACK**.
3. Enter a tag such as `v0.2.0`.
4. Leave `Create or update a GitHub release...` enabled if you want release assets uploaded automatically.
5. Leave `Sign and notarize macOS bundles...` enabled only if the Apple secrets below are configured.

### Tag-driven release

Push a tag like `v0.2.0` and the release workflow will:

1. build Windows bundles
2. build macOS bundles
3. optionally sign/notarize macOS bundles
4. create or update the GitHub Release
5. upload the packaged zip files as release assets

## Produced assets

The release workflow publishes:

- `HEXSTACK-windows-<tag>.zip`
- `HEXSTACK-macos-<tag>.zip`
- `HEXSTACK-macos-beta-<tag>.zip` when macOS signing/notarization is not configured or intentionally skipped

The macOS zip contains:

- `HEXSTACK.app`
- `HEXSTACK.vst3`
- `HEXSTACK.component`
- `READ_ME_FIRST_MAC_BETA.md`

## Optional Apple signing and notarization secrets

To enable macOS signing/notarization in GitHub Actions, add these repository secrets:

- `APPLE_DEV_ID_APP_CERT_P12_BASE64`
  - Base64-encoded `.p12` file for your **Developer ID Application** certificate.
- `APPLE_DEV_ID_APP_CERT_PASSWORD`
  - Password for that `.p12` file.
- `APPLE_ID`
  - Apple ID email used for notarization.
- `APPLE_APP_SPECIFIC_PASSWORD`
  - App-specific password for notarization.
- `APPLE_TEAM_ID`
  - Your 10-character Apple Developer Team ID.

If the certificate secrets are missing, the workflow still packages macOS bundles — they are just unsigned.

When that happens, the packaged file is named `HEXSTACK-macos-beta-<tag>.zip` so it is clearly marked as a beta/unsigned Mac release.

If the notarization secrets are missing, the workflow still packages macOS bundles — they are signed if possible, but not notarized.

## Creating the certificate secret

On a Mac, export your **Developer ID Application** certificate from Keychain Access as a `.p12` file, then convert it to base64.

Example command on macOS:

```text
base64 -i DeveloperIDApplication.p12 | pbcopy
```

Paste the copied output into the `APPLE_DEV_ID_APP_CERT_P12_BASE64` GitHub secret.

## Notes

- The release workflow targets `macos-15` and packages universal macOS binaries (`arm64` + `x86_64`).
- GitHub Releases are created or updated automatically by tag name.
- For local/private test builds you can skip notarization and still download the packaged macOS zip from workflow artifacts.
- Unsigned macOS beta builds include a bundled install guide to help testers get past Gatekeeper/quarantine friction.