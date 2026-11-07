#!/usr/bin/env bash
# Regenerate AudioScope UI screenshots at 2240 x 1400 (1120 x 700 @ 2x).
# Usage: ./scripts/shoot.sh [chrome-binary]
set -euo pipefail
cd "$(dirname "$0")/../.."

CHROME="${1:-${CHROME:-/Applications/Google Chrome.app/Contents/MacOS/Google Chrome}}"
PORT="${PORT:-8127}"
ROOT="http://127.0.0.1:${PORT}"
OUT="web/docs/screens"
mkdir -p "$OUT"

if ! curl -fsS -o /dev/null "$ROOT/" 2>/dev/null; then
  python3 -m http.server "$PORT" --bind 127.0.0.1 --directory web >/dev/null 2>&1 &
  SERVER=$!
  trap 'kill "$SERVER" 2>/dev/null || true' EXIT
  sleep 0.6
fi

shoot() {
  local name="$1" query="$2"
  local profile="/tmp/audioscope-web-${name}"
  rm -rf "$profile"
  "$CHROME" --headless=new --disable-gpu --hide-scrollbars --no-first-run \
    --no-sandbox --disable-breakpad --disable-crash-reporter \
    --user-data-dir="$profile" --window-size=1120,700 \
    --force-device-scale-factor=2 --virtual-time-budget=5000 \
    --screenshot="$PWD/$OUT/$name.png" "$ROOT/$query" >/dev/null 2>&1
  rm -rf "$profile"
  if [ ! -s "$PWD/$OUT/$name.png" ]; then
    echo "WARN: $OUT/$name.png was not written" >&2
  else
    echo "$OUT/$name.png"
  fi
}

shoot main        '#scope'
shoot spectrum    '#spectrum'
shoot xy          '#xy'
shoot measure     '#measure'
shoot repair      '#repair'
shoot reports     '#reports'
shoot calibration '#calibration'
shoot light       '?theme=light#scope'
shoot about       '#about'
shoot settings    '#settings'
