#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VST3_SOURCE="$SCRIPT_DIR/HEXSTACK.vst3"
AU_SOURCE="$SCRIPT_DIR/HEXSTACK.component"
APP_SOURCE="$SCRIPT_DIR/HEXSTACK.app"

read_choice() {
  local prompt="$1"
  shift
  local allowed=("$@")

  while true; do
    read -r -p "$prompt " value
    for allowed_value in "${allowed[@]}"; do
      if [[ "$value" == "$allowed_value" ]]; then
        printf '%s\n' "$value"
        return
      fi
    done
    echo "Please enter one of: ${allowed[*]}"
  done
}

choose_destination() {
  local label="$1"
  local default_path="$2"

  echo
  echo "$label install location:"
  echo "1) Default: $default_path"
  echo "2) Choose my own folder"

  local mode
  mode=$(read_choice "Pick 1 or 2:" 1 2)

  if [[ "$mode" == "1" ]]; then
    printf '%s\n' "$default_path"
    return
  fi

  local selected
  selected=$(osascript <<'APPLESCRIPT'
set selectedFolder to choose folder with prompt "Choose an install folder for HEXSTACK"
POSIX path of selectedFolder
APPLESCRIPT
)

  printf '%s\n' "${selected%/}"
}

install_bundle() {
  local source="$1"
  local label="$2"
  local default_path="$3"

  if [[ ! -e "$source" ]]; then
    echo "$label source not found at $source"
    exit 1
  fi

  local destination_root
  destination_root=$(choose_destination "$label" "$default_path")
  mkdir -p "$destination_root"

  local bundle_name
  bundle_name="$(basename "$source")"
  local destination="$destination_root/$bundle_name"

  rm -rf "$destination"
  ditto "$source" "$destination"
  xattr -dr com.apple.quarantine "$destination" 2>/dev/null || true

  echo "Installed $label to $destination"
}

echo "HEXSTACK macOS Installer Helper"
echo "This helper installs the files that came in this package."

install_vst3=$(read_choice "Install the VST3 plugin? (y/n):" y n)
if [[ "$install_vst3" == "y" ]]; then
  install_bundle "$VST3_SOURCE" "VST3 plugin" "$HOME/Library/Audio/Plug-Ins/VST3"
fi

install_au=$(read_choice "Install the Audio Unit plugin? (y/n):" y n)
if [[ "$install_au" == "y" ]]; then
  install_bundle "$AU_SOURCE" "Audio Unit plugin" "$HOME/Library/Audio/Plug-Ins/Components"
fi

install_app=$(read_choice "Install the standalone app? (y/n):" y n)
if [[ "$install_app" == "y" ]]; then
  install_bundle "$APP_SOURCE" "Standalone app" "$HOME/Applications"
fi

echo
echo "Done. If your DAW was open, close it and rescan plugins."
read -r -p "Press Enter to close this window..."