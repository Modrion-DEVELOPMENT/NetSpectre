#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-only
set -euo pipefail
PROJECT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
trap 'code=$?; if [ "$code" -ne 0 ]; then printf "\nInstallation failed (exit %s). Read the message above and run this installer again.\n" "$code"; fi; if [ -t 0 ]; then read -r -p "Press Enter to close..." answer || true; fi' EXIT
exec /bin/bash "$PROJECT_DIR/scripts/install-macos.sh" "$@"
