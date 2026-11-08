#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-only
set -euo pipefail
PROJECT_DIR="$(cd -- "$(dirname -- "$0")/.." && pwd)"
INSTALL_DIR="$HOME/Library/Application Support/Modrion NetSpectre"
BIN_DIR="$HOME/.local/bin"
UPDATE_PATH=1
SKIP_TOOLS=0
while [ "$#" -gt 0 ]; do
    case "$1" in
        --prefix) [ "$#" -ge 2 ] || { echo 'Missing --prefix directory'; exit 1; }; INSTALL_DIR="$2"; BIN_DIR="$2/bin"; shift 2 ;;
        --no-path) UPDATE_PATH=0; shift ;;
        --skip-tools) SKIP_TOOLS=1; shift ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done
finish() {
    code=$?
    if [ "$code" -ne 0 ]; then printf '\nInstallation failed (exit %s). Fix the error above and rerun install.command.\n' "$code"; fi
    if [ -t 0 ]; then read -r -p 'Press Enter to close...' answer || true; fi
}
trap finish EXIT
[ "$(uname -s)" = Darwin ] || { echo 'This installer requires macOS.'; exit 1; }
[ "$(id -u)" -ne 0 ] || { echo 'Run this installer as your normal user, not with sudo.'; exit 1; }
printf '\nModrion NetSpectre installer\nThis installs build tools if missing, builds the app, and adds netspro to your PATH.\nInternet access is required. Apple and Homebrew may ask for approval/passwords.\n\n'
if ! /usr/bin/xcrun --find clang++ >/dev/null 2>&1; then
    [ "$SKIP_TOOLS" -eq 0 ] || { echo 'Apple tools missing; bootstrap disabled.'; exit 1; }
    echo 'Install the Apple Command Line Tools in the dialog, then return here.'
    /usr/bin/xcode-select --install || true
    read -r -p 'When the installation finishes, press Enter to continue...' answer
    /usr/bin/xcrun --find clang++ >/dev/null || { echo 'Apple Command Line Tools are not ready.'; exit 1; }
fi
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
if ! command -v cmake >/dev/null 2>&1; then
    [ "$SKIP_TOOLS" -eq 0 ] || { echo 'CMake missing; bootstrap disabled.'; exit 1; }
    if ! command -v brew >/dev/null 2>&1; then
        installer="$(mktemp -t netspro-homebrew)"
        /usr/bin/curl --fail --show-error --location --proto '=https' --proto-redir '=https' \
            https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh --output "$installer"
        /bin/bash "$installer"
        rm -f -- "$installer"
    fi
    brew install cmake
fi
# Build in an isolated directory and keep all runtime data outside the checkout.
cmake -S "$PROJECT_DIR" -B "$PROJECT_DIR/build/install-macos" -DCMAKE_BUILD_TYPE=Release \
    -DNETSPRO_OUTPUT_DIRECTORY="$PROJECT_DIR/build/install-macos/bin"
cmake --build "$PROJECT_DIR/build/install-macos" --config Release --parallel 2
mkdir -p "$INSTALL_DIR/app/themes" "$INSTALL_DIR/app/catalog" "$BIN_DIR"
cp "$PROJECT_DIR/build/install-macos/bin/Netspro" "$INSTALL_DIR/app/Netspro.new"
chmod +x "$INSTALL_DIR/app/Netspro.new"
mv -f "$INSTALL_DIR/app/Netspro.new" "$INSTALL_DIR/app/Netspro"
for file in "$PROJECT_DIR"/themes/*.json; do
    # Keep existing user-edited themes during updates.
    [ -e "$INSTALL_DIR/app/themes/$(basename "$file")" ] || cp "$file" "$INSTALL_DIR/app/themes/"
done
for file in third_party.json homebrew.json; do cp "$PROJECT_DIR/catalog/$file" "$INSTALL_DIR/app/catalog/$file"; done
cp "$PROJECT_DIR/LICENSE" "$PROJECT_DIR/ADDITIONAL_TERMS.md" "$INSTALL_DIR/app/"
# Bash quoting supports spaces, quotes and metacharacters in user-chosen paths.
printf '#!/bin/bash\nset -e\ncd -- %q\nexec ./Netspro "$@"\n' "$INSTALL_DIR/app" > "$BIN_DIR/netspro"
chmod +x "$BIN_DIR/netspro"
# Also support case-sensitive filesystems.
if [ ! "$BIN_DIR/netspro" -ef "$BIN_DIR/Netspro" ]; then cp "$BIN_DIR/netspro" "$BIN_DIR/Netspro"; fi
if [ "$UPDATE_PATH" -eq 1 ]; then
for profile in "$HOME/.zprofile" "$HOME/.zshrc" "$HOME/.bash_profile" "$HOME/.bashrc"; do
    touch "$profile"
    if ! /usr/bin/grep -Fq '# Modrion NetSpectre PATH' "$profile"; then
        printf '\n# Modrion NetSpectre PATH\nexport PATH="$HOME/.local/bin:$PATH"\n' >> "$profile"
    fi
done
fi
printf '\nInstalled successfully. Close and reopen Terminal, then type: netspro\nYou can also type Netspro.\nApplication and data: %s/app\n' "$INSTALL_DIR"
