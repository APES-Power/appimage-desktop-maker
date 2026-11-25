#include "window.hpp"

namespace {

void on_about(GSimpleAction*, GVariant*, gpointer data)
{
    auto* app = GTK_APPLICATION(data);
    GtkWindow* parent = gtk_application_get_active_window(app);

    const char* developers[] = {"APES Power", nullptr};

    AdwDialog* about = adw_about_dialog_new();
    AdwAboutDialog* d = ADW_ABOUT_DIALOG(about);
    adw_about_dialog_set_application_name(d, adm::APP_NAME);
    adw_about_dialog_set_application_icon(d, adm::APP_ID);
    adw_about_dialog_set_version(d, adm::APP_VERSION);
    adw_about_dialog_set_developer_name(d, "APES Power");
    adw_about_dialog_set_developers(d, developers);
    adw_about_dialog_set_comments(d, "Crea entradas .desktop a partir de AppImages");
    adw_about_dialog_set_website(d, adm::APP_WEBSITE);
    adw_about_dialog_set_issue_url(d, "https://github.com/APES-Power/appimage-desktop-maker/issues");
    adw_about_dialog_add_link(d, "Repositorio en GitHub", adm::APP_WEBSITE);
    adw_about_dialog_set_license_type(d, GTK_LICENSE_GPL_2_0);
    adw_about_dialog_set_copyright(d, "© 2026 APES Power");

    adw_dialog_present(about, parent ? GTK_WIDGET(parent) : nullptr);
}

void on_quit(GSimpleAction*, GVariant*, gpointer data)
{
    // Close windows so their close-request handlers save settings.
    auto* app = GTK_APPLICATION(data);
    GList* windows = g_list_copy(gtk_application_get_windows(app));
    for (GList* l = windows; l; l = l->next)
        gtk_window_close(GTK_WINDOW(l->data));
    g_list_free(windows);
}

void on_startup(GApplication* app, gpointer)
{
    static const GActionEntry actions[] = {
        {"about", on_about, nullptr, nullptr, nullptr, {0}},
        {"quit", on_quit, nullptr, nullptr, nullptr, {0}},
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app), actions, G_N_ELEMENTS(actions), app);

    const char* quit_accels[] = {"<Control>q", nullptr};
    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.quit", quit_accels);
    const char* close_accels[] = {"<Control>w", nullptr};
    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "window.close", close_accels);

    gtk_window_set_default_icon_name(adm::APP_ID);
}

void on_activate(GApplication* app, gpointer)
{
    GtkWindow* win = gtk_application_get_active_window(GTK_APPLICATION(app));
    if (win)
        gtk_window_present(win);
    else
        adm::create_main_window(ADW_APPLICATION(app));
}

}  // namespace

int main(int argc, char** argv)
{
    AdwApplication* app = adw_application_new(adm::APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "startup", G_CALLBACK(on_startup), nullptr);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
    const int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
