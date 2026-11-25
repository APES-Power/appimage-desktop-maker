#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace adm {

namespace fs = std::filesystem;

// Options used to build a single .desktop entry.
struct EntryOptions {
    std::string name;        // Name= (if empty, derived from the file name)
    std::string comment;     // Comment=
    std::string categories;  // Categories= (e.g. "Utility;")
    bool terminal = false;   // Terminal=
    std::string startup_wm_class;  // StartupWMClass= (omitted if empty)
};

struct GenerateResult {
    bool ok = false;
    bool skipped = false;          // file existed and overwrite was disabled
    fs::path desktop_path;
    std::optional<fs::path> icon;  // icon used, if one was found
    std::string error;
};

// True if the path has an .AppImage extension (case-insensitive).
bool is_appimage(const fs::path& p);

// Sorted list of AppImages directly inside `dir`.
std::vector<fs::path> list_appimages(const fs::path& dir);

// File name without the .AppImage extension ("Foo-1.2-x86_64").
std::string appimage_stem(const fs::path& appimage);

// Human-friendly name guessed from the file name ("Foo").
std::string guess_app_name(const fs::path& appimage);

// Looks in `icons_dir` for an image with the same stem as the AppImage.
std::optional<fs::path> find_icon(const fs::path& icons_dir, const fs::path& appimage);

// Builds the text of the .desktop file.
std::string build_desktop_entry(const fs::path& appimage,
                                const std::optional<fs::path>& icon,
                                const EntryOptions& opts);

// Writes <desktop_dir>/<stem>.desktop for the given AppImage.
GenerateResult generate_entry(const fs::path& appimage,
                              const fs::path& icons_dir,
                              const fs::path& desktop_dir,
                              const EntryOptions& opts,
                              bool overwrite,
                              bool make_executable);

}  // namespace adm
