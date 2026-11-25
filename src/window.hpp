#pragma once

#include <adwaita.h>

namespace adm {

inline constexpr const char* APP_ID = "io.github.APES_Power.AppImageDesktopMaker";
inline constexpr const char* APP_NAME = "AppImage Desktop Maker";
inline constexpr const char* APP_VERSION = "1.0.0";
inline constexpr const char* APP_WEBSITE = "https://github.com/APES-Power/appimage-desktop-maker";

// Builds and presents the main window for `app`.
void create_main_window(AdwApplication* app);

}  // namespace adm
