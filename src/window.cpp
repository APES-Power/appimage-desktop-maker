#include "window.hpp"

#include "desktop_entry.hpp"

#include <glib/gstdio.h>

#include <string>
#include <vector>

namespace adm {

namespace {

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

struct Window {
    AdwApplicationWindow* window = nullptr;
    AdwToastOverlay* toasts = nullptr;

    // Shared directories
    AdwEntryRow* appimage_dir_row = nullptr;
    AdwEntryRow* icons_dir_row = nullptr;
    AdwEntryRow* desktop_dir_row = nullptr;

    // "Un AppImage" tab
    AdwComboRow* single_combo = nullptr;
    GtkStringList* single_model = nullptr;
    AdwActionRow* single_icon_row = nullptr;
    GtkImage* single_icon_image = nullptr;
    AdwEntryRow* single_name = nullptr;
    AdwEntryRow* single_comment = nullptr;
    AdwEntryRow* single_categories = nullptr;
    AdwSwitchRow* single_terminal = nullptr;
    AdwEntryRow* single_wm_class = nullptr;
    AdwSwitchRow* single_overwrite = nullptr;
    AdwSwitchRow* single_exec = nullptr;
    GtkButton* single_button = nullptr;

    // "Toda la carpeta" tab
    AdwPreferencesGroup* folder_group = nullptr;
    GtkListBox* folder_list = nullptr;
    AdwEntryRow* folder_categories = nullptr;
    AdwSwitchRow* folder_terminal = nullptr;
    AdwSwitchRow* folder_wm_class = nullptr;
    AdwSwitchRow* folder_overwrite = nullptr;
    AdwSwitchRow* folder_exec = nullptr;
    GtkButton* folder_button = nullptr;

    std::vector<fs::path> appimages;
    guint refresh_source = 0;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

fs::path expand_path(const char* text)
{
    std::string s = text ? text : "";
    // Trim surrounding whitespace.
    const auto b = s.find_first_not_of(" \t\n");
    const auto e = s.find_last_not_of(" \t\n");
    s = (b == std::string::npos) ? "" : s.substr(b, e - b + 1);

    if (s == "~")
        return g_get_home_dir();
    if (s.rfind("~/", 0) == 0)
        return fs::path(g_get_home_dir()) / s.substr(2);
    return s;
}

fs::path row_path(AdwEntryRow* row)
{
    return expand_path(gtk_editable_get_text(GTK_EDITABLE(row)));
}

std::string editable_text(AdwEntryRow* row)
{
    return gtk_editable_get_text(GTK_EDITABLE(row));
}

void toast(Window* w, const std::string& msg)
{
    AdwToast* t = adw_toast_new(msg.c_str());
    adw_toast_set_timeout(t, 4);
    adw_toast_overlay_add_toast(w->toasts, t);
}

void set_dir_error(AdwEntryRow* row, bool error)
{
    if (error)
        gtk_widget_add_css_class(GTK_WIDGET(row), "error");
    else
        gtk_widget_remove_css_class(GTK_WIDGET(row), "error");
}

// Refreshes the desktop menu cache, if the tool is available.
void update_desktop_database(const fs::path& dir)
{
    gchar* prog = g_find_program_in_path("update-desktop-database");
    if (!prog)
        return;
    const std::string d = dir.string();
    const gchar* argv[] = {prog, "-q", d.c_str(), nullptr};
    g_spawn_async(nullptr, const_cast<gchar**>(argv), nullptr,
                  static_cast<GSpawnFlags>(G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL),
                  nullptr, nullptr, nullptr, nullptr);
    g_free(prog);
}

GtkWidget* make_icon_image(const std::optional<fs::path>& icon)
{
    GtkWidget* img;
    if (icon)
        img = gtk_image_new_from_file(icon->c_str());
    else
        img = gtk_image_new_from_icon_name("application-x-executable");
    gtk_image_set_pixel_size(GTK_IMAGE(img), 32);
    return img;
}

// ---------------------------------------------------------------------------
// Settings (stored in ~/.config/appimage-desktop-maker/settings.ini)
// ---------------------------------------------------------------------------

std::string settings_file()
{
    gchar* p = g_build_filename(g_get_user_config_dir(), "appimage-desktop-maker", "settings.ini", nullptr);
    std::string s = p;
    g_free(p);
    return s;
}

void load_settings(Window* w)
{
    const std::string home = g_get_home_dir();
    std::string appimages = home + "/Applications";
    std::string icons = home + "/Applications/icons";
    // Not g_get_user_data_dir(): inside Flatpak that points to ~/.var/app/<id>/data,
    // which the host desktop never reads.
    std::string desktop = home + "/.local/share/applications";

    GKeyFile* kf = g_key_file_new();
    if (g_key_file_load_from_file(kf, settings_file().c_str(), G_KEY_FILE_NONE, nullptr)) {
        auto get = [&](const char* key, std::string& out) {
            gchar* v = g_key_file_get_string(kf, "directories", key, nullptr);
            if (v) {
                out = v;
                g_free(v);
            }
        };
        get("appimages", appimages);
        get("icons", icons);
        get("desktop", desktop);
    }
    g_key_file_unref(kf);

    gtk_editable_set_text(GTK_EDITABLE(w->appimage_dir_row), appimages.c_str());
    gtk_editable_set_text(GTK_EDITABLE(w->icons_dir_row), icons.c_str());
    gtk_editable_set_text(GTK_EDITABLE(w->desktop_dir_row), desktop.c_str());
}

void save_settings(Window* w)
{
    GKeyFile* kf = g_key_file_new();
    g_key_file_set_string(kf, "directories", "appimages", editable_text(w->appimage_dir_row).c_str());
    g_key_file_set_string(kf, "directories", "icons", editable_text(w->icons_dir_row).c_str());
    g_key_file_set_string(kf, "directories", "desktop", editable_text(w->desktop_dir_row).c_str());

    const std::string file = settings_file();
    gchar* dir = g_path_get_dirname(file.c_str());
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);
    g_key_file_save_to_file(kf, file.c_str(), nullptr);
    g_key_file_unref(kf);
}

// ---------------------------------------------------------------------------
// Refreshing the AppImage lists
// ---------------------------------------------------------------------------

void update_single_details(Window* w)
{
    const guint idx = adw_combo_row_get_selected(w->single_combo);
    const bool valid = idx != GTK_INVALID_LIST_POSITION && idx < w->appimages.size();
    gtk_widget_set_sensitive(GTK_WIDGET(w->single_button), valid);

    // StartupWMClass is specific to each app, so don't carry it over.
    gtk_editable_set_text(GTK_EDITABLE(w->single_wm_class), "");

    if (!valid) {
        gtk_editable_set_text(GTK_EDITABLE(w->single_name), "");
        adw_action_row_set_subtitle(w->single_icon_row, "Selecciona un AppImage");
        gtk_image_set_from_icon_name(w->single_icon_image, "application-x-executable");
        return;
    }

    const fs::path& app = w->appimages[idx];
    gtk_editable_set_text(GTK_EDITABLE(w->single_name), guess_app_name(app).c_str());

    const auto icon = find_icon(row_path(w->icons_dir_row), app);
    if (icon) {
        adw_action_row_set_subtitle(w->single_icon_row, icon->filename().c_str());
        gtk_image_set_from_file(w->single_icon_image, icon->c_str());
    } else {
        const std::string msg = "No se encontró «" + appimage_stem(app) + ".png/.svg/…» — se usará un icono genérico";
        adw_action_row_set_subtitle(w->single_icon_row, msg.c_str());
        gtk_image_set_from_icon_name(w->single_icon_image, "application-x-executable");
    }
}

void refresh_lists(Window* w)
{
    const fs::path dir = row_path(w->appimage_dir_row);
    std::error_code ec;
    set_dir_error(w->appimage_dir_row, !dir.empty() && !fs::is_directory(dir, ec));
    const fs::path icons = row_path(w->icons_dir_row);
    set_dir_error(w->icons_dir_row, !icons.empty() && !fs::is_directory(icons, ec));

    // Keep the current selection in the single tab if it still exists.
    std::string previous;
    const guint prev_idx = adw_combo_row_get_selected(w->single_combo);
    if (prev_idx != GTK_INVALID_LIST_POSITION && prev_idx < w->appimages.size())
        previous = w->appimages[prev_idx].filename().string();

    w->appimages = list_appimages(dir);

    // --- Single tab: combo model
    std::vector<const char*> names;
    std::vector<std::string> storage;
    storage.reserve(w->appimages.size());
    guint select = w->appimages.empty() ? GTK_INVALID_LIST_POSITION : 0;
    for (size_t i = 0; i < w->appimages.size(); ++i) {
        storage.push_back(w->appimages[i].filename().string());
        if (storage.back() == previous)
            select = static_cast<guint>(i);
    }
    for (const auto& s : storage)
        names.push_back(s.c_str());
    names.push_back(nullptr);

    const guint old_n = g_list_model_get_n_items(G_LIST_MODEL(w->single_model));
    gtk_string_list_splice(w->single_model, 0, old_n, names.data());
    adw_combo_row_set_selected(w->single_combo, select);
    update_single_details(w);

    // --- Folder tab: list of rows
    gtk_list_box_remove_all(w->folder_list);
    for (const auto& app : w->appimages) {
        const auto icon = find_icon(icons, app);
        GtkWidget* row = adw_action_row_new();
        adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), app.filename().c_str());
        const std::string sub = icon ? "Icono: " + icon->filename().string() : "Sin icono (se usará uno genérico)";
        adw_action_row_set_subtitle(ADW_ACTION_ROW(row), sub.c_str());
        adw_action_row_add_prefix(ADW_ACTION_ROW(row), make_icon_image(icon));
        if (!icon) {
            GtkWidget* warn = gtk_image_new_from_icon_name("dialog-warning-symbolic");
            gtk_widget_add_css_class(warn, "warning");
            gtk_widget_set_tooltip_text(warn, "No hay un icono con el mismo nombre");
            adw_action_row_add_suffix(ADW_ACTION_ROW(row), warn);
        }
        gtk_list_box_append(w->folder_list, row);
    }

    const std::string desc = w->appimages.empty()
                                 ? std::string("No hay AppImages en el directorio seleccionado")
                                 : std::to_string(w->appimages.size()) + " AppImage(s) encontrados en " + dir.string();
    adw_preferences_group_set_description(w->folder_group, desc.c_str());
    gtk_widget_set_sensitive(GTK_WIDGET(w->folder_button), !w->appimages.empty());
}

gboolean refresh_timeout(gpointer data)
{
    auto* w = static_cast<Window*>(data);
    w->refresh_source = 0;
    refresh_lists(w);
    return G_SOURCE_REMOVE;
}

void schedule_refresh(Window* w)
{
    if (w->refresh_source)
        g_source_remove(w->refresh_source);
    w->refresh_source = g_timeout_add(250, refresh_timeout, w);
}

// ---------------------------------------------------------------------------
// Folder chooser
// ---------------------------------------------------------------------------

struct PickData {
    Window* w;
    AdwEntryRow* row;
};

void on_folder_selected(GObject* source, GAsyncResult* res, gpointer data)
{
    auto* pd = static_cast<PickData*>(data);
    GFile* file = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source), res, nullptr);
    if (file) {
        gchar* path = g_file_get_path(file);
        if (path) {
            gtk_editable_set_text(GTK_EDITABLE(pd->row), path);
            g_free(path);
        }
        g_object_unref(file);
    }
    delete pd;
}

void on_pick_clicked(GtkButton* button, gpointer data)
{
    auto* w = static_cast<Window*>(data);
    auto* row = static_cast<AdwEntryRow*>(g_object_get_data(G_OBJECT(button), "row"));

    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, adw_preferences_row_get_title(ADW_PREFERENCES_ROW(row)));
    gtk_file_dialog_set_modal(dialog, TRUE);

    const fs::path current = row_path(row);
    std::error_code ec;
    if (!current.empty() && fs::is_directory(current, ec)) {
        GFile* f = g_file_new_for_path(current.c_str());
        gtk_file_dialog_set_initial_folder(dialog, f);
        g_object_unref(f);
    }

    gtk_file_dialog_select_folder(dialog, GTK_WINDOW(w->window), nullptr, on_folder_selected,
                                  new PickData{w, row});
    g_object_unref(dialog);
}

AdwEntryRow* make_dir_row(Window* w, AdwPreferencesGroup* group, const char* title, const char* tooltip)
{
    auto* row = ADW_ENTRY_ROW(adw_entry_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
    gtk_widget_set_tooltip_text(GTK_WIDGET(row), tooltip);

    GtkWidget* btn = gtk_button_new_from_icon_name("folder-open-symbolic");
    gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(btn, "flat");
    gtk_widget_set_tooltip_text(btn, "Elegir carpeta…");
    g_object_set_data(G_OBJECT(btn), "row", row);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_pick_clicked), w);
    adw_entry_row_add_suffix(row, btn);

    adw_preferences_group_add(group, GTK_WIDGET(row));
    return row;
}

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

bool check_desktop_dir(Window* w)
{
    if (row_path(w->desktop_dir_row).empty()) {
        set_dir_error(w->desktop_dir_row, true);
        toast(w, "Indica el directorio donde guardar los .desktop");
        return false;
    }
    set_dir_error(w->desktop_dir_row, false);
    return true;
}

void on_single_generate(GtkButton*, gpointer data)
{
    auto* w = static_cast<Window*>(data);
    const guint idx = adw_combo_row_get_selected(w->single_combo);
    if (idx == GTK_INVALID_LIST_POSITION || idx >= w->appimages.size() || !check_desktop_dir(w))
        return;

    EntryOptions opts;
    opts.name = editable_text(w->single_name);
    opts.comment = editable_text(w->single_comment);
    opts.categories = editable_text(w->single_categories);
    opts.terminal = adw_switch_row_get_active(w->single_terminal);
    opts.startup_wm_class = editable_text(w->single_wm_class);

    const fs::path desktop_dir = row_path(w->desktop_dir_row);
    const auto res = generate_entry(w->appimages[idx], row_path(w->icons_dir_row), desktop_dir, opts,
                                    adw_switch_row_get_active(w->single_overwrite),
                                    adw_switch_row_get_active(w->single_exec));

    if (!res.ok)
        toast(w, "Error: " + res.error);
    else if (res.skipped)
        toast(w, res.desktop_path.filename().string() + " ya existe (activa «Sobrescribir» para reemplazarlo)");
    else {
        update_desktop_database(desktop_dir);
        toast(w, "Creado " + res.desktop_path.string());
    }
}

void on_folder_generate(GtkButton*, gpointer data)
{
    auto* w = static_cast<Window*>(data);
    if (w->appimages.empty() || !check_desktop_dir(w))
        return;

    EntryOptions opts;
    opts.categories = editable_text(w->folder_categories);
    opts.terminal = adw_switch_row_get_active(w->folder_terminal);

    const fs::path desktop_dir = row_path(w->desktop_dir_row);
    const fs::path icons = row_path(w->icons_dir_row);
    const bool overwrite = adw_switch_row_get_active(w->folder_overwrite);
    const bool exec = adw_switch_row_get_active(w->folder_exec);
    const bool wm_class = adw_switch_row_get_active(w->folder_wm_class);

    int created = 0, skipped = 0, failed = 0, no_icon = 0;
    std::string first_error;
    for (const auto& app : w->appimages) {
        opts.startup_wm_class = wm_class ? guess_app_name(app) : std::string();
        const auto res = generate_entry(app, icons, desktop_dir, opts, overwrite, exec);
        if (!res.ok) {
            ++failed;
            if (first_error.empty())
                first_error = res.error;
        } else if (res.skipped) {
            ++skipped;
        } else {
            ++created;
            if (!res.icon)
                ++no_icon;
        }
    }

    if (created > 0)
        update_desktop_database(desktop_dir);

    std::string msg = std::to_string(created) + " creadas";
    if (skipped)
        msg += ", " + std::to_string(skipped) + " omitidas (ya existían)";
    if (no_icon)
        msg += ", " + std::to_string(no_icon) + " sin icono";
    if (failed)
        msg += ", " + std::to_string(failed) + " con error: " + first_error;
    toast(w, msg);
}

// ---------------------------------------------------------------------------
// Page builders
// ---------------------------------------------------------------------------

GtkWidget* make_action_button(const char* label)
{
    GtkWidget* btn = gtk_button_new_with_label(label);
    gtk_widget_add_css_class(btn, "pill");
    gtk_widget_add_css_class(btn, "suggested-action");
    gtk_widget_set_halign(btn, GTK_ALIGN_CENTER);
    return btn;
}

AdwSwitchRow* make_switch(AdwPreferencesGroup* group, const char* title, const char* subtitle, bool active)
{
    auto* row = ADW_SWITCH_ROW(adw_switch_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
    if (subtitle)
        adw_action_row_set_subtitle(ADW_ACTION_ROW(row), subtitle);
    adw_switch_row_set_active(row, active);
    adw_preferences_group_add(group, GTK_WIDGET(row));
    return row;
}

AdwEntryRow* make_entry(AdwPreferencesGroup* group, const char* title, const char* text)
{
    auto* row = ADW_ENTRY_ROW(adw_entry_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
    if (text)
        gtk_editable_set_text(GTK_EDITABLE(row), text);
    adw_preferences_group_add(group, GTK_WIDGET(row));
    return row;
}

GtkWidget* build_single_page(Window* w)
{
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);

    // AppImage selection
    auto* sel = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(sel, "AppImage");
    adw_preferences_group_set_description(sel, "Elige uno de los AppImages del directorio indicado arriba");

    w->single_model = gtk_string_list_new(nullptr);
    w->single_combo = ADW_COMBO_ROW(adw_combo_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(w->single_combo), "Archivo");
    adw_combo_row_set_model(w->single_combo, G_LIST_MODEL(w->single_model));
    adw_combo_row_set_enable_search(w->single_combo, TRUE);
    adw_preferences_group_add(sel, GTK_WIDGET(w->single_combo));
    g_signal_connect_swapped(w->single_combo, "notify::selected", G_CALLBACK(update_single_details), w);

    w->single_icon_row = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(w->single_icon_row), "Icono");
    w->single_icon_image = GTK_IMAGE(make_icon_image(std::nullopt));
    adw_action_row_add_prefix(w->single_icon_row, GTK_WIDGET(w->single_icon_image));
    adw_preferences_group_add(sel, GTK_WIDGET(w->single_icon_row));
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(sel));

    // Entry fields
    auto* fields = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(fields, "Entrada .desktop");
    w->single_name = make_entry(fields, "Nombre", nullptr);
    w->single_comment = make_entry(fields, "Comentario", nullptr);
    w->single_categories = make_entry(fields, "Categorías", "Utility;");
    w->single_terminal = make_switch(fields, "Ejecutar en terminal", nullptr, false);
    w->single_wm_class = make_entry(fields, "StartupWMClass (opcional)", nullptr);
    gtk_widget_set_tooltip_text(GTK_WIDGET(w->single_wm_class),
                                "Clase de ventana de la app, para que el dock agrupe sus ventanas con este "
                                "lanzador. Puedes verla con «xprop WM_CLASS». Déjalo vacío para omitirlo.");
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(fields));

    // Options
    auto* options = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(options, "Opciones");
    w->single_overwrite = make_switch(options, "Sobrescribir si ya existe", nullptr, true);
    w->single_exec = make_switch(options, "Marcar el AppImage como ejecutable", nullptr, true);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(options));

    w->single_button = GTK_BUTTON(make_action_button("Crear entrada .desktop"));
    g_signal_connect(w->single_button, "clicked", G_CALLBACK(on_single_generate), w);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(w->single_button));

    return box;
}

GtkWidget* build_folder_page(Window* w)
{
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);

    w->folder_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(w->folder_group, "AppImages");

    w->folder_list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(w->folder_list, GTK_SELECTION_NONE);
    gtk_widget_add_css_class(GTK_WIDGET(w->folder_list), "boxed-list");
    GtkWidget* placeholder = gtk_label_new("Ningún AppImage encontrado");
    gtk_widget_add_css_class(placeholder, "dim-label");
    gtk_widget_set_margin_top(placeholder, 18);
    gtk_widget_set_margin_bottom(placeholder, 18);
    gtk_list_box_set_placeholder(w->folder_list, placeholder);
    adw_preferences_group_add(w->folder_group, GTK_WIDGET(w->folder_list));
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(w->folder_group));

    auto* options = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(options, "Opciones");
    adw_preferences_group_set_description(options,
                                          "El nombre de cada entrada se deduce del nombre del archivo");
    w->folder_categories = make_entry(options, "Categorías", "Utility;");
    w->folder_terminal = make_switch(options, "Ejecutar en terminal", nullptr, false);
    w->folder_wm_class = make_switch(options, "Añadir StartupWMClass",
                                     "Usa el nombre deducido de cada archivo (p. ej. «Krita»)", false);
    w->folder_overwrite = make_switch(options, "Sobrescribir las que ya existan", nullptr, false);
    w->folder_exec = make_switch(options, "Marcar los AppImages como ejecutables", nullptr, true);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(options));

    w->folder_button = GTK_BUTTON(make_action_button("Crear todas las entradas"));
    g_signal_connect(w->folder_button, "clicked", G_CALLBACK(on_folder_generate), w);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(w->folder_button));

    return box;
}

// ---------------------------------------------------------------------------
// Window lifecycle
// ---------------------------------------------------------------------------

gboolean on_close_request(GtkWindow*, gpointer data)
{
    save_settings(static_cast<Window*>(data));
    return FALSE;
}

void on_window_destroy(gpointer data)
{
    auto* w = static_cast<Window*>(data);
    if (w->refresh_source)
        g_source_remove(w->refresh_source);
    delete w;
}

GMenuModel* build_primary_menu()
{
    GMenu* menu = g_menu_new();
    g_menu_append(menu, "_Acerca de AppImage Desktop Maker", "app.about");
    GMenu* root = g_menu_new();
    g_menu_append_section(root, nullptr, G_MENU_MODEL(menu));
    g_object_unref(menu);
    GMenu* quit = g_menu_new();
    g_menu_append(quit, "_Salir", "app.quit");
    g_menu_append_section(root, nullptr, G_MENU_MODEL(quit));
    g_object_unref(quit);
    return G_MENU_MODEL(root);
}

}  // namespace

void create_main_window(AdwApplication* app)
{
    auto* w = new Window();

    w->window = ADW_APPLICATION_WINDOW(adw_application_window_new(GTK_APPLICATION(app)));
    gtk_window_set_title(GTK_WINDOW(w->window), APP_NAME);
    gtk_window_set_default_size(GTK_WINDOW(w->window), 680, 820);
    gtk_widget_set_size_request(GTK_WIDGET(w->window), 360, 400);
    g_object_set_data_full(G_OBJECT(w->window), "adm-state", w, on_window_destroy);
    g_signal_connect(w->window, "close-request", G_CALLBACK(on_close_request), w);

    // Header bar
    GtkWidget* header = adw_header_bar_new();
    GtkWidget* menu_btn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_btn), "open-menu-symbolic");
    gtk_menu_button_set_primary(GTK_MENU_BUTTON(menu_btn), TRUE);
    gtk_widget_set_tooltip_text(menu_btn, "Menú principal");
    GMenuModel* menu = build_primary_menu();
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_btn), menu);
    g_object_unref(menu);
    adw_header_bar_pack_end(ADW_HEADER_BAR(header), menu_btn);

    // Content
    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
    gtk_widget_set_margin_top(content, 24);
    gtk_widget_set_margin_bottom(content, 24);
    gtk_widget_set_margin_start(content, 12);
    gtk_widget_set_margin_end(content, 12);

    auto* dirs = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(dirs, "Directorios");
    adw_preferences_group_set_description(
        dirs, "Los iconos deben llamarse igual que su AppImage (p. ej. «Foo.AppImage» → «Foo.png» o «Foo.svg»)");
    w->appimage_dir_row = make_dir_row(w, dirs, "Directorio de AppImages", "Carpeta que contiene los archivos .AppImage");
    w->icons_dir_row = make_dir_row(w, dirs, "Directorio de iconos", "Carpeta con los iconos para los .desktop");
    w->desktop_dir_row = make_dir_row(w, dirs, "Destino de los .desktop", "Normalmente ~/.local/share/applications");
    gtk_box_append(GTK_BOX(content), GTK_WIDGET(dirs));

    // Tabs
    GtkWidget* stack = adw_view_stack_new();
    adw_view_stack_set_vhomogeneous(ADW_VIEW_STACK(stack), FALSE);
    adw_view_stack_add_titled_with_icon(ADW_VIEW_STACK(stack), build_single_page(w), "single", "Un AppImage",
                                        "application-x-executable-symbolic");
    adw_view_stack_add_titled_with_icon(ADW_VIEW_STACK(stack), build_folder_page(w), "folder", "Toda la carpeta",
                                        "folder-symbolic");

    GtkWidget* switcher = adw_inline_view_switcher_new();
    adw_inline_view_switcher_set_stack(ADW_INLINE_VIEW_SWITCHER(switcher), ADW_VIEW_STACK(stack));
    adw_inline_view_switcher_set_display_mode(ADW_INLINE_VIEW_SWITCHER(switcher),
                                              ADW_INLINE_VIEW_SWITCHER_BOTH);
    adw_inline_view_switcher_set_homogeneous(ADW_INLINE_VIEW_SWITCHER(switcher), TRUE);
    gtk_box_append(GTK_BOX(content), switcher);
    gtk_box_append(GTK_BOX(content), stack);

    GtkWidget* clamp = adw_clamp_new();
    adw_clamp_set_maximum_size(ADW_CLAMP(clamp), 640);
    adw_clamp_set_child(ADW_CLAMP(clamp), content);

    GtkWidget* scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), clamp);
    gtk_widget_set_vexpand(scroller, TRUE);

    GtkWidget* toolbar = adw_toolbar_view_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), scroller);

    w->toasts = ADW_TOAST_OVERLAY(adw_toast_overlay_new());
    adw_toast_overlay_set_child(w->toasts, toolbar);
    adw_application_window_set_content(w->window, GTK_WIDGET(w->toasts));

    // Load settings, then react to changes.
    load_settings(w);
    g_signal_connect_swapped(w->appimage_dir_row, "changed", G_CALLBACK(schedule_refresh), w);
    g_signal_connect_swapped(w->icons_dir_row, "changed", G_CALLBACK(schedule_refresh), w);
    g_signal_connect_swapped(w->desktop_dir_row, "changed",
                             G_CALLBACK(+[](Window* win) { set_dir_error(win->desktop_dir_row, false); }), w);
    refresh_lists(w);

    gtk_window_present(GTK_WINDOW(w->window));
}

}  // namespace adm
