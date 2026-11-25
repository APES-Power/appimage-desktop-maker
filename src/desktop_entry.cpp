#include "desktop_entry.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>

namespace adm {

namespace {

std::string to_lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Escapes a value for a desktop-entry "string" key (spec: \s \n \t \r \\).
std::string escape_string(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\t': out += "\\t"; break;
        case '\r': out += "\\r"; break;
        default: out += c;
        }
    }
    // A leading space must be written as \s.
    if (!out.empty() && out.front() == ' ')
        out.replace(0, 1, "\\s");
    return out;
}

// Quotes a single argument for the Exec key. Inside double quotes the
// characters " ` $ \ must be backslash-escaped; then the whole value is
// escaped again as a regular string (handled by escape_string).
std::string quote_exec_arg(const std::string& arg)
{
    std::string out = "\"";
    for (char c : arg) {
        if (c == '"' || c == '`' || c == '$' || c == '\\')
            out += '\\';
        out += c;
    }
    out += '"';
    return out;
}

bool is_noise_token(const std::string& tok)
{
    static const std::array<const char*, 12> noise = {
        "x86_64", "x86-64", "amd64", "aarch64", "arm64", "armhf",
        "i386",   "i686",   "linux", "appimage", "x64",  "universal"};
    const std::string low = to_lower(tok);
    for (const char* n : noise)
        if (low == n)
            return true;
    // Version-looking tokens: "1.2.3", "v2", "2024"...
    if (!low.empty() && std::isdigit(static_cast<unsigned char>(low[0])))
        return true;
    if (low.size() > 1 && low[0] == 'v' && std::isdigit(static_cast<unsigned char>(low[1])))
        return true;
    return false;
}

}  // namespace

bool is_appimage(const fs::path& p)
{
    return to_lower(p.extension().string()) == ".appimage";
}

std::vector<fs::path> list_appimages(const fs::path& dir)
{
    std::vector<fs::path> result;
    std::error_code ec;
    if (dir.empty() || !fs::is_directory(dir, ec))
        return result;

    for (const auto& entry : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
        std::error_code ec2;
        if (entry.is_regular_file(ec2) && is_appimage(entry.path()))
            result.push_back(entry.path());
    }
    std::sort(result.begin(), result.end(), [](const fs::path& a, const fs::path& b) {
        return to_lower(a.filename().string()) < to_lower(b.filename().string());
    });
    return result;
}

std::string appimage_stem(const fs::path& appimage)
{
    return appimage.stem().string();
}

std::string guess_app_name(const fs::path& appimage)
{
    const std::string stem = appimage_stem(appimage);

    // "x86_64" / "x86-64" would be split in two below; turn it into one noise token.
    std::string s = stem;
    for (const char* arch : {"x86_64", "x86-64", "X86_64", "X86-64"}) {
        for (auto pos = s.find(arch); pos != std::string::npos; pos = s.find(arch))
            s.replace(pos, 6, "x64");
    }

    // Split on '-', '_' and ' ', and keep tokens until we hit a version/arch token.
    std::vector<std::string> kept;
    std::string tok;
    auto flush = [&]() -> bool {
        if (tok.empty())
            return true;
        if (!kept.empty() && is_noise_token(tok))
            return false;
        kept.push_back(tok);
        tok.clear();
        return true;
    };
    bool done = false;
    for (char c : s) {
        if (c == '-' || c == '_' || c == ' ') {
            if (!flush()) {
                done = true;
                break;
            }
        } else {
            tok += c;
        }
    }
    if (!done)
        flush();

    std::string name;
    for (const auto& k : kept) {
        if (!name.empty())
            name += ' ';
        name += k;
    }
    return name.empty() ? stem : name;
}

std::optional<fs::path> find_icon(const fs::path& icons_dir, const fs::path& appimage)
{
    std::error_code ec;
    if (icons_dir.empty() || !fs::is_directory(icons_dir, ec))
        return std::nullopt;

    static const std::array<const char*, 6> exts = {".svg", ".png", ".xpm", ".jpg", ".jpeg", ".ico"};
    const std::string stem = appimage_stem(appimage);

    // Fast path: exact file names, in order of preference.
    for (const char* ext : exts) {
        fs::path candidate = icons_dir / (stem + ext);
        if (fs::is_regular_file(candidate, ec))
            return candidate;
    }

    // Slow path: case-insensitive comparison of stem and extension.
    const std::string stem_low = to_lower(stem);
    std::optional<fs::path> best;
    size_t best_rank = exts.size();
    for (const auto& entry : fs::directory_iterator(icons_dir, fs::directory_options::skip_permission_denied, ec)) {
        std::error_code ec2;
        if (!entry.is_regular_file(ec2))
            continue;
        const auto& p = entry.path();
        if (to_lower(p.stem().string()) != stem_low)
            continue;
        const std::string ext = to_lower(p.extension().string());
        for (size_t i = 0; i < exts.size(); ++i) {
            if (ext == exts[i] && i < best_rank) {
                best = p;
                best_rank = i;
            }
        }
    }
    return best;
}

std::string build_desktop_entry(const fs::path& appimage,
                                const std::optional<fs::path>& icon,
                                const EntryOptions& opts)
{
    const fs::path abs = fs::absolute(appimage);
    const std::string name = opts.name.empty() ? guess_app_name(appimage) : opts.name;

    std::string categories = opts.categories;
    if (!categories.empty() && categories.back() != ';')
        categories += ';';

    std::ostringstream out;
    out << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Version=1.5\n"
        << "Name=" << escape_string(name) << "\n";
    if (!opts.comment.empty())
        out << "Comment=" << escape_string(opts.comment) << "\n";
    out << "Exec=" << escape_string(quote_exec_arg(abs.string())) << " %U\n"
        << "TryExec=" << escape_string(abs.string()) << "\n";
    if (icon)
        out << "Icon=" << escape_string(fs::absolute(*icon).string()) << "\n";
    else
        out << "Icon=application-x-executable\n";
    out << "Terminal=" << (opts.terminal ? "true" : "false") << "\n";
    if (!categories.empty())
        out << "Categories=" << escape_string(categories) << "\n";
    if (!opts.startup_wm_class.empty())
        out << "StartupWMClass=" << escape_string(opts.startup_wm_class) << "\n";
    out << "X-AppImage-Path=" << escape_string(abs.string()) << "\n";
    return out.str();
}

GenerateResult generate_entry(const fs::path& appimage,
                              const fs::path& icons_dir,
                              const fs::path& desktop_dir,
                              const EntryOptions& opts,
                              bool overwrite,
                              bool make_executable)
{
    GenerateResult res;
    std::error_code ec;

    if (!fs::is_regular_file(appimage, ec)) {
        res.error = "No existe el AppImage: " + appimage.string();
        return res;
    }
    if (desktop_dir.empty()) {
        res.error = "No se ha indicado el directorio de destino de los .desktop";
        return res;
    }
    fs::create_directories(desktop_dir, ec);
    if (ec) {
        res.error = "No se pudo crear " + desktop_dir.string() + ": " + ec.message();
        return res;
    }

    res.desktop_path = desktop_dir / (appimage_stem(appimage) + ".desktop");
    res.icon = find_icon(icons_dir, appimage);

    if (!overwrite && fs::exists(res.desktop_path, ec)) {
        res.skipped = true;
        res.ok = true;
        return res;
    }

    if (make_executable) {
        fs::permissions(appimage,
                        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                        fs::perm_options::add, ec);
        // Not fatal: the file may live on a read-only or foreign filesystem.
    }

    std::ofstream f(res.desktop_path, std::ios::trunc);
    if (!f) {
        res.error = "No se pudo escribir " + res.desktop_path.string();
        return res;
    }
    f << build_desktop_entry(appimage, res.icon, opts);
    f.close();
    if (!f) {
        res.error = "Error al escribir " + res.desktop_path.string();
        return res;
    }

    // .desktop files are commonly marked executable so file managers trust them.
    fs::permissions(res.desktop_path, fs::perms::owner_exec, fs::perm_options::add, ec);

    res.ok = true;
    return res;
}

}  // namespace adm
