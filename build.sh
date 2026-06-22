#!/usr/bin/env bash
#
# Convenience build (and optional install) script for Vela.
#
#   ./build.sh           Configure + compile into ./build, binary at ./build/vela
#   ./build.sh install   Build, then install system-wide and register as a
#                        selectable default video player (needs sudo).
#
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

echo ">> Configuring..."
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release

echo ">> Building with ${JOBS} job(s)..."
cmake --build "${BUILD_DIR}" -j "${JOBS}"

echo ">> Done. Binary: ${BUILD_DIR}/vela"

if [[ "${1:-}" == "install" ]]; then
    echo ">> Installing (you may be prompted for your password)..."
    sudo cmake --install "${BUILD_DIR}"

    # Refresh desktop + icon caches so Vela shows up in the launcher and in
    # the "Open With" / default-application menus.
    if command -v update-desktop-database >/dev/null 2>&1; then
        sudo update-desktop-database /usr/local/share/applications || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        sudo gtk-update-icon-cache -f /usr/local/share/icons/hicolor || true
    fi

    echo ""
    echo ">> Installed. To make Vela your default video player, run:"
    echo "     xdg-mime default vela.desktop video/mp4 video/x-matroska video/x-msvideo"
    echo "   (or right-click any video > Properties > Open With > Vela > Set as default)"
fi
