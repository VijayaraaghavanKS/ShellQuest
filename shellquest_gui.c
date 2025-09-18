#include <gtk/gtk.h>
#include <vte/vte.h>

static void on_exit_button_clicked(GtkWidget *widget, gpointer data) {
    system("konsole -e zsh"); // Use KDE's konsole
    gtk_main_quit();
}

static void update_progress_label(GtkWidget *label) {
    FILE *fp = fopen("/tmp/shellquest/.quest_progress", "r");
    if (fp) {
        int xp, level, quests, ls_stage;
        fscanf(fp, "%d %d %d %d", &xp, &level, &quests, &ls_stage);
        fclose(fp);
        char text[128];
        snprintf(text, sizeof(text), "Progress: XP=%d, Level=%d, Quests=%d/8", xp, level, quests);
        gtk_label_set_text(GTK_LABEL(label), text);
    }
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "ShellQuest Tutorial");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *help_label = gtk_label_new("Welcome to ShellQuest! Type 'teach ls' below. Use buttons for help.");
    gtk_box_pack_start(GTK_BOX(vbox), help_label, FALSE, FALSE, 0);

    GtkWidget *progress_label = gtk_label_new("Progress: XP=0, Level=1, Quests=0/8");
    gtk_box_pack_start(GTK_BOX(vbox), progress_label, FALSE, FALSE, 0);

    GtkWidget *terminal = vte_terminal_new();
    char *shell_args[] = {"/usr/local/bin/shellquest", NULL};
    vte_terminal_spawn_async(VTE_TERMINAL(terminal), VTE_PTY_DEFAULT, NULL, shell_args, NULL,
                            G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, -1, NULL, NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), terminal, TRUE, TRUE, 0);

    GtkWidget *exit_button = gtk_button_new_with_label("Exit to Standard Terminal");
    g_signal_connect(exit_button, "clicked", G_CALLBACK(on_exit_button_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), exit_button, FALSE, FALSE, 0);

    g_timeout_add_seconds(1, (GSourceFunc)update_progress_label, progress_label);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
