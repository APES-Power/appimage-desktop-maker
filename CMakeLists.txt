cmake_minimum_required(VERSION 3.16)
project(appimage-desktop-maker VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4>=4.12)
pkg_check_modules(ADW REQUIRED IMPORTED_TARGET libadwaita-1>=1.7)
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)

set(APP_ID io.github.APES_Power.AppImageDesktopMaker)
set(RES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/res)
set(RESOURCES_C ${CMAKE_CURRENT_BINARY_DIR}/resources.c)

add_custom_command(
    OUTPUT ${RESOURCES_C}
    COMMAND ${GLIB_COMPILE_RESOURCES} --generate-source --target=${RESOURCES_C}
            --sourcedir=${RES_DIR} ${RES_DIR}/resources.gresource.xml
    DEPENDS ${RES_DIR}/resources.gresource.xml ${RES_DIR}/logos/${APP_ID}.svg
    COMMENT "Compiling GResources")
set_source_files_properties(${RESOURCES_C} PROPERTIES LANGUAGE CXX)

add_executable(appimage-desktop-maker
    src/main.cpp
    src/window.cpp
    src/desktop_entry.cpp
    ${RESOURCES_C})

target_compile_options(appimage-desktop-maker PRIVATE -Wall -Wextra)
target_link_libraries(appimage-desktop-maker PRIVATE PkgConfig::GTK4 PkgConfig::ADW)

install(TARGETS appimage-desktop-maker RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${RES_DIR}/${APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${RES_DIR}/${APP_ID}.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES ${RES_DIR}/logos/${APP_ID}.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
foreach(size 64 128 256 512)
    install(FILES ${RES_DIR}/logos/${APP_ID}-${size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
            RENAME ${APP_ID}.png)
endforeach()
