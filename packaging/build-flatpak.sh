#!/usr/bin/env bash
# Builds AppImage Desktop Maker as a Flatpak bundle.
#
# Usage: packaging/build-flatpak.sh [--install]
# Output: AppImage-Desktop-Maker-<version>.flatpak in the project root.
#
# Uses flatpak-builder with packaging/flatpak/*.yml when it is available
# (or as the org.flatpak.Builder flatpak); otherwise falls back to plain
# `flatpak build*` commands. Either way it needs org.gnome.Sdk//$RUNTIME_VERSION
# and org.gnome.Platform//$RUNTIME_VERSION installed.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_ID="io.github.APES_Power.AppImageDesktopMaker"
MANIFEST="$ROOT/packaging/flatpak/$APP_ID.yml"
RUNTIME_VERSION="$(sed -n "s/^runtime-version: *'\{0,1\}\([^']*\)'\{0,1\}/\1/p" "$MANIFEST")"
VERSION="$(sed -n 's/^project(.*VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
# Outside the source tree: flatpak needs a filesystem with symlink support.
WORK="${FLATPAK_WORKDIR:-${XDG_CACHE_HOME:-$HOME/.cache}/appimage-desktop-maker/flatpak}"
APPDIR="$WORK/app"
REPO="$WORK/repo"
BUNDLE="$ROOT/AppImage-Desktop-Maker-$VERSION.flatpak"

# Keep in sync with finish-args in the manifest.
FINISH_ARGS=(--share=ipc --socket=wayland --socket=fallback-x11 --device=dri --filesystem=host)

if command -v flatpak-builder >/dev/null; then
    BUILDER=(flatpak-builder)
elif flatpak info org.flatpak.Builder >/dev/null 2>&1; then
    BUILDER=(flatpak run org.flatpak.Builder)
else
    BUILDER=()
fi

mkdir -p "$WORK"

if ((${#BUILDER[@]})); then
    echo ">> Compilando con ${BUILDER[*]}"
    "${BUILDER[@]}" --force-clean --repo="$REPO" --state-dir="$WORK/.flatpak-builder" \
        "$APPDIR" "$MANIFEST"
else
    echo ">> flatpak-builder no encontrado; usando flatpak build"
    rm -rf "$APPDIR" "$WORK/cmake"
    flatpak build-init "$APPDIR" "$APP_ID" org.gnome.Sdk org.gnome.Platform "$RUNTIME_VERSION"

    BUILD=(flatpak build --filesystem="$ROOT" --filesystem="$WORK" "$APPDIR")
    "${BUILD[@]}" cmake -S "$ROOT" -B "$WORK/cmake" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/app >/dev/null
    "${BUILD[@]}" cmake --build "$WORK/cmake"
    "${BUILD[@]}" cmake --install "$WORK/cmake" --strip >/dev/null

    flatpak build-finish "$APPDIR" --command=appimage-desktop-maker "${FINISH_ARGS[@]}"
    flatpak build-export "$REPO" "$APPDIR"
fi

echo ">> Generando bundle"
flatpak build-bundle "$REPO" "$BUNDLE" "$APP_ID" \
    --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo
echo ">> Listo: $BUNDLE"

if [[ "${1:-}" == "--install" ]]; then
    flatpak install --user -y --reinstall "$BUNDLE"
fi
