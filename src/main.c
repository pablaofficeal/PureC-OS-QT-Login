#include <stdio.h>
#include <string.h>
#include <time.h>

#include "pg/pg.h"
#include "session.h"

static struct {
    struct pg_window *win;
    struct pg_widget *clock;
    struct pg_widget *date;
    struct pg_widget *users;
    struct pg_widget *name;
    struct pg_widget *password;
    struct pg_widget *error;
    struct pg_widget *login;
    struct session_user list[SESSION_MAX_USERS];
    int count;
    int session;
    uint32_t clock_timer;
    uint32_t session_timer;
} ui;

static void build(void);

static void bg_draw(struct pg_widget *w, struct pg_canvas *c)
{
    const struct pg_theme *t = pg_theme();
    struct pg_rect r = w->rect;
    uint32_t top = pgc_lighten(t->bg, t->light ? 4 : -12);
    uint32_t bottom = pgc_mix(t->window, t->accent, 18);
    pgc_vgrad(c, r.x, r.y, r.w, r.h, top, bottom);
    pgc_circle(c, r.x + r.w * 82 / 100, r.y + r.h * 18 / 100, r.w * 28 / 100, t->accent, 18);
    pgc_circle(c, r.x + r.w * 12 / 100, r.y + r.h * 90 / 100, r.w * 22 / 100, t->info, 14);
}

static void bg_measure(struct pg_widget *w, int *width, int *height)
{
    pg_widget_measure(w->first, width, height);
}

static void bg_arrange(struct pg_widget *w)
{
    if (w->first)
        pg_widget_place(w->first, w->rect.x, w->rect.y, w->rect.w, w->rect.h);
}

static const struct pg_widget_class bg_class = {
    .name = "login-background",
    .measure = bg_measure,
    .arrange = bg_arrange,
    .draw = bg_draw,
};

static const char *const weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *const months[] = {"January", "February", "March",     "April",   "May",      "June",
                                     "July",    "August",   "September", "October", "November", "December"};

static bool update_clock(void *data)
{
    (void)data;
    if (!ui.win)
        return true;
    time_t now = time(NULL);
    struct tm tm;
    if (!localtime_r(&now, &tm))
        return true;
    char buf[64];
    snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
    pg_label_set_text(ui.clock, buf);
    snprintf(buf, sizeof(buf), "%s, %d %s %d", weekdays[(tm.tm_wday % 7 + 7) % 7], tm.tm_mday,
             months[(tm.tm_mon % 12 + 12) % 12], tm.tm_year + 1900);
    pg_label_set_text(ui.date, buf);
    return true;
}

static const char *selected_user(void)
{
    if (!ui.count)
        return pg_entry_text(ui.name);
    int i = pg_list_selected(ui.users);
    return i >= 0 && i < ui.count ? ui.list[i].name : "";
}

static bool enable_login(void *data)
{
    (void)data;
    if (ui.login)
        pg_widget_set_enabled(ui.login, true);
    return false;
}

static bool check_session(void *data)
{
    (void)data;
    if (ui.session <= 0 || !session_finished(ui.session))
        return true;
    ui.session = -1;
    ui.session_timer = 0;
    build();
    pg_hold(false);
    return false;
}

static void try_login(struct pg_widget *w, void *data)
{
    (void)w;
    (void)data;
    if (!pg_widget_is_enabled(ui.login))
        return;
    const char *error = "";
    char user[64];
    snprintf(user, sizeof(user), "%s", selected_user());
    char *end = user + strlen(user);
    while (end > user && end[-1] == ' ')
        *--end = 0;
    bool ok = session_authenticate(user, pg_entry_text(ui.password), &error);
    pg_entry_set_text(ui.password, "");
    if (!ok) {
        pg_label_set_text(ui.error, error);
        pg_widget_set_enabled(ui.login, false);
        pg_timer_add(1500, enable_login, NULL);
        return;
    }
    pg_label_set_text(ui.error, "");
    char msg[128];
    int pid = session_start(user, msg, sizeof(msg));
    if (pid < 0) {
        pg_label_set_text(ui.error, msg);
        return;
    }
    ui.session = pid;
    pg_hold(true);
    pg_window_destroy(ui.win);
    ui.win = NULL;
    ui.login = NULL;
    ui.session_timer = pg_timer_add(500, check_session, NULL);
}

static void user_changed(struct pg_widget *w, void *data)
{
    (void)w;
    (void)data;
    pg_label_set_text(ui.error, "");
    pg_entry_set_text(ui.password, "");
    pg_widget_grab_focus(ui.password);
}

static void name_enter(struct pg_widget *w, void *data)
{
    (void)w;
    (void)data;
    pg_widget_grab_focus(ui.password);
}

static void power(struct pg_widget *w, void *data)
{
    (void)w;
    session_power(data != NULL);
}

static bool on_key(struct pg_window *win, const struct pg_event *e, void *data)
{
    (void)win;
    (void)data;
    if (e->key == PG_KEY_ESCAPE) {
        pg_entry_set_text(ui.password, "");
        pg_label_set_text(ui.error, "");
        return true;
    }
    if ((e->key == PG_KEY_UP || e->key == PG_KEY_DOWN) && ui.count > 0) {
        int i = pg_list_selected(ui.users);
        if (i < 0)
            i = 0;
        i = (i + (e->key == PG_KEY_DOWN ? 1 : ui.count - 1)) % ui.count;
        pg_list_set_selected(ui.users, i);
        pg_entry_set_text(ui.password, "");
        pg_label_set_text(ui.error, "");
        return true;
    }
    return false;
}

static bool on_close(struct pg_window *win, void *data)
{
    (void)win;
    (void)data;
    return false;
}

static struct pg_widget *hspace(void)
{
    struct pg_widget *s = pg_spacer_new();
    pg_widget_set_expand(s, true, false);
    return s;
}

static void build(void)
{
    int sw = 1024, sh = 768;
    pg_screen_size(&sw, &sh);
    ui.win = pg_window_new_at("Login", 0, 0, sw, sh, PG_WINDOW_FRAMELESS);
    if (!ui.win) {
        pg_quit(1);
        return;
    }
    pg_window_fullscreen(ui.win);
    pg_window_on_key(ui.win, on_key, NULL);
    pg_window_on_close(ui.win, on_close, NULL);

    struct pg_widget *bg = pg_widget_new(&bg_class);
    struct pg_widget *root = pg_vbox(0);
    pg_box_set_padding(root, 32, 28, 32, 24);
    pg_widget_add(bg, root);

    struct pg_widget *top = pg_hbox(12);
    pg_widget_add(top, pg_label_new_font("PureC OS", PGF_HEAD, PG_COLOR_ACCENT));
    pg_widget_add(top, hspace());
    char host[64];
    session_host_name(host, sizeof(host));
    struct pg_widget *host_label = pg_label_new_font(host, PGF_BODY, PG_COLOR_TEXT3);
    pg_widget_set_align(host_label, PG_ALIGN_FILL, PG_ALIGN_CENTER);
    pg_widget_add(top, host_label);
    pg_widget_add(root, top);
    pg_widget_add(root, pg_spacer_new());

    ui.clock = pg_label_new_font("", PGF_DIGITS, PG_COLOR_TEXT);
    pg_label_set_text_align(ui.clock, PG_ALIGN_CENTER);
    ui.date = pg_label_new_font("", PGF_MEDIUM, PG_COLOR_TEXT2);
    pg_label_set_text_align(ui.date, PG_ALIGN_CENTER);
    pg_widget_set_margins(ui.date, 0, 4, 0, 28);
    pg_widget_add(root, ui.clock);
    pg_widget_add(root, ui.date);

    struct pg_widget *card = pg_card_new(NULL, NULL, PG_NO_ICON);
    pg_card_set_padding(card, 24);
    pg_card_set_spacing(card, 10);
    pg_widget_set_size(card, 400, -1);
    pg_widget_set_align(card, PG_ALIGN_CENTER, PG_ALIGN_START);

    ui.count = session_users(ui.list, SESSION_MAX_USERS);
    ui.users = pg_list_new();
    for (int i = 0; i < ui.count; i++) {
        const struct session_user *u = &ui.list[i];
        bool plain = !u->full_name[0] || strcmp(u->full_name, u->name) == 0;
        pg_list_add(ui.users, u->uid == 0 ? PGI_SHIELD_CHECK : PGI_ACCOUNT_CIRCLE, plain ? u->name : u->full_name,
                    plain ? NULL : u->name);
    }
    if (ui.count)
        pg_list_set_selected(ui.users, 0);
    pg_widget_on(ui.users, PG_SIGNAL_CHANGED, user_changed, NULL);
    pg_widget_set_visible(ui.users, ui.count > 0);
    pg_widget_add(card, ui.users);

    ui.name = pg_entry_new("User name");
    pg_widget_set_visible(ui.name, ui.count == 0);
    pg_widget_on(ui.name, PG_SIGNAL_ACTIVATE, name_enter, NULL);
    pg_widget_add(card, ui.name);

    ui.password = pg_entry_new(session_live() ? "Live system: no password needed" : "Password");
    pg_entry_set_password(ui.password, true);
    pg_widget_on(ui.password, PG_SIGNAL_ACTIVATE, try_login, NULL);
    pg_widget_add(card, ui.password);

    ui.error = pg_label_new_font("", PGF_BODY, PG_COLOR_DANGER);
    pg_label_set_text_align(ui.error, PG_ALIGN_CENTER);
    pg_widget_add(card, ui.error);

    ui.login = pg_button_new("Log in", PG_NO_ICON, PG_BUTTON_PRIMARY);
    pg_widget_on(ui.login, PG_SIGNAL_CLICKED, try_login, NULL);
    pg_widget_add(card, ui.login);
    pg_widget_add(root, card);
    struct pg_widget *bottom_space = pg_spacer_new();
    pg_widget_add(root, bottom_space);

    struct pg_widget *bottom = pg_hbox(10);
    pg_widget_add(bottom, hspace());
    struct pg_widget *restart = pg_button_new("Restart", PGI_RESTART, PG_BUTTON_SECONDARY);
    struct pg_widget *off = pg_button_new("Power off", PGI_POWER, PG_BUTTON_SECONDARY);
    pg_widget_on(restart, PG_SIGNAL_CLICKED, power, (void *)1);
    pg_widget_on(off, PG_SIGNAL_CLICKED, power, NULL);
    pg_widget_add(bottom, restart);
    pg_widget_add(bottom, off);
    pg_widget_add(root, bottom);

    pg_window_set_child(ui.win, bg);
    pg_widget_grab_focus(ui.count ? ui.password : ui.name);
    update_clock(NULL);
}

int main(int argc, char **argv)
{
    if (!pg_init(argc, argv)) {
        fprintf(stderr, "login: cannot connect to videod\n");
        return 1;
    }
    ui.session = -1;
    build();
    if (!ui.win)
        return 1;
    ui.clock_timer = pg_timer_add(1000, update_clock, NULL);
    int code = pg_run();
    pg_shutdown();
    return code;
}
