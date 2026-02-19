#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# serve-local-update.sh — Local update server for testing the helix-screen
#                          download → install → restart flow end-to-end.
#
# OVERVIEW
# --------
# Builds a Pi package, generates a manifest.json with a bumped version
# (patch +1 over VERSION.txt), and serves both over HTTP. The running
# helix-screen binary sees the manifest version as newer than itself and
# triggers the full self-update path without touching VERSION.txt.
#
# By default the patch version is bumped on every run so helix-screen
# always treats it as a new release. Use --no-bump to serve the exact
# VERSION.txt version instead.
#
# QUICK START
# -----------
#   # 1. Set your Pi's address (once, add to ~/.zshrc or ~/.bashrc):
#   export HELIX_TEST_PI_USER=pi
#   export HELIX_TEST_PI_HOST=helixscreen.local   # or an IP address
#
#   # 2. First time only — configure the Pi to use dev channel:
#   ./scripts/serve-local-update.sh --configure-pi
#
#   # 3. Every subsequent test iteration:
#   ./scripts/serve-local-update.sh
#
#   helix-screen will check the manifest, see a newer version, download
#   the tarball, run install.sh, and restart itself.
#
# ENVIRONMENT VARIABLES
# ---------------------
#   HELIX_TEST_PI_HOST   Pi hostname or IP  (default: helixscreen.local)
#   HELIX_TEST_PI_USER   Pi SSH username    (default: pi)
#
# OPTIONS
# -------
#   --configure-pi   SSH into the Pi and write dev channel + dev_url into
#                    ~/helixscreen/config/settings.json. Run once per Pi
#                    (or after a factory reset). Requires passwordless SSH
#                    or an SSH agent with the Pi's key loaded.
#   --no-bump        Serve the exact version from VERSION.txt instead of
#                    patch+1. Useful if you manually set a higher version.
#   --no-build       Skip package.sh and use the existing dist/ tarball.
#                    Must be paired with the same version the tarball was
#                    built with (default: bumped version).
#   --port PORT      HTTP port to listen on (default: 8765).
#
# DEPENDENCIES
# ------------
#   - make / build toolchain (for package.sh → cross-compiled pi binary)
#   - python3 (for the HTTP server and Pi settings.json patch)
#   - ssh + ssh-agent or ~/.ssh/config with key for $HELIX_TEST_PI_USER@$HELIX_TEST_PI_HOST
#   - rsync (used by package.sh)
#
# NOTES
# -----
#   - The bumped version is only used for the tarball filename and manifest.
#     VERSION.txt is never modified.
#   - After install the Pi binary still reports VERSION.txt's version, so
#     the next run will again offer an update — intentional for iteration.
#   - To reset the Pi back to the stable channel, re-run --configure-pi
#     after removing the dev_url key, or delete settings.json on the Pi.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# ── Defaults (overridable via env) ────────────────────────────────────────────
PI_HOST="${HELIX_TEST_PI_HOST:-helixscreen.local}"
PI_USER="${HELIX_TEST_PI_USER:-pi}"
PORT=8765
BUILD=1
BUMP=1
CONFIGURE_PI=0

# ── Args ──────────────────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case $1 in
        --configure-pi) CONFIGURE_PI=1; shift ;;
        --no-build)     BUILD=0; shift ;;
        --no-bump)      BUMP=0; shift ;;
        --port)         PORT=$2; shift 2 ;;
        --help|-h)
            sed -n '2,/^set -/p' "$0" | grep '^#' | sed 's/^# \?//'
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# ── Version ───────────────────────────────────────────────────────────────────
BASE_VERSION="$(tr -d '[:space:]' < "$PROJECT_DIR/VERSION.txt")"  # e.g. 0.10.4

bump_patch() {
    local ver=$1
    local major minor patch
    IFS='.' read -r major minor patch <<< "$ver"
    echo "${major}.${minor}.$((patch + 1))"
}

if [[ $BUMP -eq 1 ]]; then
    TEST_VERSION="$(bump_patch "$BASE_VERSION")"
else
    TEST_VERSION="$BASE_VERSION"
fi

VERSION="v${TEST_VERSION}"
TARBALL_NAME="helixscreen-pi-${VERSION}.tar.gz"
TARBALL_PATH="$PROJECT_DIR/dist/$TARBALL_NAME"

# ── Local IP ──────────────────────────────────────────────────────────────────
LOCAL_IP=$(ipconfig getifaddr en0 2>/dev/null || \
           ip route get 1 2>/dev/null | awk '{print $7; exit}' || \
           echo "127.0.0.1")
BASE_URL="http://${LOCAL_IP}:${PORT}"

echo ""
echo "  serve-local-update"
echo "  ══════════════════"
echo "  Base version : $BASE_VERSION  (VERSION.txt — unchanged)"
echo "  Test version : $TEST_VERSION  $([ $BUMP -eq 1 ] && echo "(patch bumped)" || echo "(no bump)")"
echo "  Tarball      : $TARBALL_NAME"
echo "  Serving at   : $BASE_URL"
echo "  Pi target    : ${PI_USER}@${PI_HOST}"
echo ""

# ── Build ─────────────────────────────────────────────────────────────────────
if [[ $BUILD -eq 1 ]]; then
    echo "[serve-local-update] Building pi package..."
    "$SCRIPT_DIR/package.sh" pi --version "$VERSION"
    echo ""
fi

if [[ ! -f "$TARBALL_PATH" ]]; then
    echo "ERROR: Tarball not found: $TARBALL_PATH"
    echo ""
    echo "  Expected: $TARBALL_PATH"
    echo "  Either run without --no-build, or build manually:"
    echo "    scripts/package.sh pi --version $VERSION"
    exit 1
fi

# ── Manifest ──────────────────────────────────────────────────────────────────
MANIFEST_PATH="$PROJECT_DIR/dist/manifest.json"
cat > "$MANIFEST_PATH" <<EOF
{
  "version": "${VERSION}",
  "channel": "dev",
  "assets": [
    {
      "platform": "pi",
      "filename": "${TARBALL_NAME}",
      "url": "${BASE_URL}/${TARBALL_NAME}"
    }
  ]
}
EOF
echo "[serve-local-update] Manifest → $MANIFEST_PATH"

# ── Configure Pi ──────────────────────────────────────────────────────────────
if [[ $CONFIGURE_PI -eq 1 ]]; then
    echo "[serve-local-update] Configuring ${PI_USER}@${PI_HOST} ..."
    ssh "${PI_USER}@${PI_HOST}" "python3 -c \"
import json, os
path = os.path.expanduser('~/helixscreen/config/settings.json')
data = {}
if os.path.exists(path):
    with open(path) as f:
        data = json.load(f)
data.setdefault('update', {})
data['update']['channel'] = 2
data['update']['dev_url'] = '${BASE_URL}/'
with open(path, 'w') as f:
    json.dump(data, f, indent=2)
print('  update/channel =', data['update']['channel'], '(dev)')
print('  update/dev_url =', data['update']['dev_url'])
\""
    echo ""
    echo "[serve-local-update] Pi configured — restart helix-screen to pick up settings."
    echo "  To revert: remove update/dev_url from ~/helixscreen/config/settings.json on the Pi."
    echo ""
fi

# ── Serve ─────────────────────────────────────────────────────────────────────
echo ""
echo "  Ready"
echo "  ─────────────────────────────────────────────────"
echo "  Manifest : ${BASE_URL}/manifest.json"
echo "  Tarball  : ${BASE_URL}/${TARBALL_NAME}"
echo ""
echo "  Pi expects (set via --configure-pi if not already done):"
echo "    update/channel  = 2"
echo "    update/dev_url  = \"${BASE_URL}/\""
echo ""
echo "  Press Ctrl+C to stop."
echo "  ─────────────────────────────────────────────────"
echo ""

cd "$PROJECT_DIR/dist"
python3 -m http.server "$PORT"
