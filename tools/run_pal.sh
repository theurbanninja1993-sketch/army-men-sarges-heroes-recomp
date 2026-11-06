#!/usr/bin/env bash
# Experimental Linux PAL English60 build; no game data is distributed with the port.
set -euo pipefail
port_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if (( $# < 1 )); then
    echo "Usage: $0 PAL_DISC_IMAGE [game options...]" >&2
    exit 2
fi
pal_disc="$(realpath -- "$1")"
shift
pal_exe="$port_root/build-release/game/armymensh_pal_controls"
if [[ ! -x "$pal_exe" ]]; then
    echo "Build armymensh_pal_controls first; see docs/findings/pal-bringup.md." >&2
    exit 2
fi
echo "Experimental PAL build: select English and 60 Hz in the original startup menu." >&2
exec "$pal_exe" --config "$port_root/game/armymensh_pal_probe.toml" \
    --disc "$pal_disc" --window "$@"
