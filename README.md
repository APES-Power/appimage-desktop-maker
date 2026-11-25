# appimage-desktop-maker

<img src="res/logos/io.github.APES_Power.AppImageDesktopMaker-128.png" align="right" alt="logo">

Simple GTK4 + libadwaita program to create `.desktop` entries for AppImages.

- Choose the AppImages folder, the icons folder and the destination folder for the `.desktop` files
  (default `~/.local/share/applications`).
- **Un AppImage** tab: pick a single AppImage and customise name, comment, categories and terminal.
- **Toda la carpeta** tab: generate entries for every AppImage in the folder at once.
- Icons are matched by file name: `Foo-1.2.AppImage` → `Foo-1.2.svg` / `.png` / `.xpm` / `.jpg` / `.ico`.
  If no icon is found, a generic one is used.
- Directories are remembered in `~/.config/appimage-desktop-maker/settings.ini`.

## Build

Dependencies: CMake ≥ 3.16, a C++17 compiler, GTK ≥ 4.12, libadwaita ≥ 1.7.

```sh
# Debian/Ubuntu
sudo apt install build-essential cmake libgtk-4-dev libadwaita-1-dev

cmake -S . -B build
cmake --build build
./build/appimage-desktop-maker

# optional
sudo cmake --install build
```

## Flatpak

Requires `org.gnome.Sdk//51` and `org.gnome.Platform//51` from Flathub.

```sh
packaging/build-flatpak.sh            # → AppImage-Desktop-Maker-<version>.flatpak
packaging/build-flatpak.sh --install  # build and install for the current user
```

The script uses `flatpak-builder` with
[packaging/flatpak/io.github.APES_Power.AppImageDesktopMaker.yml](packaging/flatpak/io.github.APES_Power.AppImageDesktopMaker.yml)
when available, and falls back to plain `flatpak build` otherwise. The app runs with
`--filesystem=host`, because the generated `.desktop` files must contain real paths to
the AppImages and icons.

## License

GPL-2.0 — see [LICENSE](LICENSE).
