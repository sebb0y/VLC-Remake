#!/usr/bin/env bash
#
# Register Vela as the default handler for common video formats on Linux
# (freedesktop / XDG desktops: GNOME, KDE, XFCE, etc.).
#
# Run this AFTER installing Vela (./build.sh install), so that vela.desktop
# exists in a known applications directory.
#
set -euo pipefail

MIME_TYPES=(
    video/mp4
    video/x-matroska
    video/x-msvideo
    video/quicktime
    video/webm
    video/mpeg
    video/x-flv
    video/3gpp
    video/ogg
    video/x-ms-wmv
    video/x-m4v
    video/mp2t
    video/x-ms-asf
)

echo ">> Setting Vela as the default video player for ${#MIME_TYPES[@]} formats..."
xdg-mime default vela.desktop "${MIME_TYPES[@]}"
echo ">> Done. Double-click any video and it should open in Vela."
