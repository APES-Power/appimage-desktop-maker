# AppImage Desktop Maker

<img src="res/logos/io.github.APES_Power.AppImageDesktopMaker-128.png" align="right" alt="logo">

Programa sencillo desarrollado con GTK4 y libadwaita para crear archivos `.desktop` para aplicaciones AppImage.

- Selecciona la carpeta de AppImages, la carpeta de iconos y la carpeta de destino de los archivos `.desktop` (por defecto: `~/.local/share/applications`).
- **Una sola AppImage:** selecciona un único archivo AppImage y personaliza su nombre, descripción, categorías y ejecución en terminal.
- **Toda la carpeta:** genera entradas automáticamente para todos los archivos AppImage de una carpeta.
- Los iconos se buscan utilizando el nombre del archivo: `Foo-1.2.AppImage` → `Foo-1.2.svg` / `.png` / `.xpm` / `.jpg` / `.ico`.
  Si no se encuentra ningún icono, se utiliza uno genérico.
- Las carpetas seleccionadas se guardan en `~/.config/appimage-desktop-maker/settings.ini`.

## Compilación

Dependencias: CMake ≥ 3.16, un compilador compatible con C++17, GTK ≥ 4.12 y libadwaita ≥ 1.7.

```sh
# Debian/Ubuntu
sudo apt install build-essential cmake libgtk-4-dev libadwaita-1-dev

cmake -S . -B build
cmake --build build
./build/appimage-desktop-maker

# Opcional
sudo cmake --install build
```

## Flatpak

Requiere `org.gnome.Sdk//51` y `org.gnome.Platform//51` de Flathub.

```sh
packaging/build-flatpak.sh            # → AppImage-Desktop-Maker-<version>.flatpak
packaging/build-flatpak.sh --install  # Compilar e instalar para el usuario actual
```

El script utiliza `flatpak-builder` con el archivo
[packaging/flatpak/io.github.APES_Power.AppImageDesktopMaker.yml](packaging/flatpak/io.github.APES_Power.AppImageDesktopMaker.yml)
si está disponible. En caso contrario, utiliza como alternativa el comando `flatpak build` directamente.

La aplicación se ejecuta con el permiso `--filesystem=host`, ya que los archivos `.desktop` generados deben contener las rutas reales de las aplicaciones AppImage y de sus iconos.

## Licencia

GPL-2.0 — consulta el archivo [LICENSE](LICENSE).
