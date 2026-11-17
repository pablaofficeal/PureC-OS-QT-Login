#include "session.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "accounts.h"

static const char *session_program = "/usr/bin/desktop-rs";

static int read_file(const char *path, char *buf, int cap)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    int total = 0;
    while (total < cap - 1) {
        ssize_t n = read(fd, buf + total, (size_t)(cap - 1 - total));
        if (n <= 0)
            break;
        total += (int)n;
    }
    close(fd);
    buf[total] = 0;
    return total;
}

static int split(char *line, char sep, char **fields, int max)
{
    int n = 0;
    fields[n++] = line;
    for (char *p = line; *p && n < max; p++) {
        if (*p == sep) {
            *p = 0;
            fields[n++] = p + 1;
        }
    }
    return n;
}

int session_users(struct session_user *out, int max)
{
    static char buf[16384];
    if (read_file("/etc/passwd", buf, sizeof(buf)) < 0)
        return 0;
    int count = 0;
    struct session_user root;
    bool have_root = false;
    for (char *line = buf; line && *line;) {
        char *end = strchr(line, '\n');
        if (end)
            *end = 0;
        char *f[8];
        if (split(line, ':', f, 8) >= 7 && !strstr(f[6], "nologin") && !strstr(f[6], "false")) {
            char *stop = NULL;
            unsigned long uid = strtoul(f[2], &stop, 10);
            if (stop != f[2] && !*stop) {
                struct session_user u;
                memset(&u, 0, sizeof(u));
                snprintf(u.name, sizeof(u.name), "%s", f[0]);
                char *comma = strchr(f[4], ',');
                if (comma)
                    *comma = 0;
                snprintf(u.full_name, sizeof(u.full_name), "%s", f[4]);
                u.uid = (uint32_t)uid;
                if (uid == 0) {
                    root = u;
                    have_root = true;
                } else if (uid >= 1000 && uid < 60000 && count < max - 1) {
                    out[count++] = u;
                }
            }
        }
        line = end ? end + 1 : NULL;
    }
    if (have_root && count < max)
        out[count++] = root;
    return count;
}

bool session_live(void)
{
    return access("/etc/shadow", F_OK) != 0;
}

void session_host_name(char *out, int cap)
{
    char buf[128];
    int n = read_file("/etc/hostname", buf, sizeof(buf));
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == ' ' || buf[n - 1] == '\r'))
        buf[--n] = 0;
    snprintf(out, (size_t)cap, "%s", n > 0 ? buf : "purec");
}

bool session_authenticate(const char *name, const char *password, const char **error)
{
    if (!name || !*name) {
        *error = "Choose a user";
        return false;
    }
    if (session_live())
        return true;
    struct user_info info;
    bool ok = account_login(name, password, &info);
    if (!ok)
        *error = "Wrong user name or password";
    return ok;
}

static void add_env(char **envp, int *n, char storage[][200], const char *key, const char *value)
{
    snprintf(storage[*n], 200, "%s=%s", key, value);
    envp[*n] = storage[*n];
    (*n)++;
}

int session_start(const char *name, char *error, int cap)
{
    struct user_info who;
    if (pc_user_lookup(name, 0, &who) != 0) {
        snprintf(error, (size_t)cap, "Unknown user %s", name);
        return -1;
    }
    if (access(session_program, X_OK) != 0) {
        snprintf(error, (size_t)cap, "The desktop is not installed (pkg -S userspace)");
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        snprintf(error, (size_t)cap, "Cannot start the session");
        return -1;
    }
    if (pid > 0)
        return (int)pid;
    setsid();
    for (int s = 1; s < 32; ++s)
        signal(s, SIG_DFL);
    if (pc_setgid(who.gid) != 0 || pc_setuid(who.uid) != 0)
        _exit(126);
    const char *home = who.home[0] ? who.home : "/";
    if (chdir(home) != 0) {
        home = "/";
        if (chdir("/") != 0)
            _exit(126);
    }
    const char *shell = who.shell[0] && access(who.shell, X_OK) == 0 ? who.shell : "/bin/sh";
    static char storage[12][200];
    char *envp[13];
    int k = 0;
    add_env(envp, &k, storage, "HOME", home);
    add_env(envp, &k, storage, "USER", who.name);
    add_env(envp, &k, storage, "LOGNAME", who.name);
    add_env(envp, &k, storage, "SHELL", shell);
    add_env(envp, &k, storage, "PATH",
            who.uid == 0 ? "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin" : "/usr/local/bin:/usr/bin:/bin");
    add_env(envp, &k, storage, "TERM", "xterm-256color");
    add_env(envp, &k, storage, "PS1",
            who.uid == 0 ? "\\[\\e[1;31m\\]\\u@\\h\\[\\e[0m\\]:\\[\\e[1;34m\\]\\w\\[\\e[0m\\]# "
                         : "\\[\\e[1;32m\\]\\u@\\h\\[\\e[0m\\]:\\[\\e[1;34m\\]\\w\\[\\e[0m\\]$ ");
    add_env(envp, &k, storage, "XDG_SESSION_TYPE", "videod");
    envp[k] = NULL;
    char *argv[] = {(char *)"desktop-rs", NULL};
    execve(session_program, argv, envp);
    _exit(127);
}

bool session_finished(int pid)
{
    int status = 0;
    pid_t r = waitpid((pid_t)pid, &status, WNOHANG);
    return r == pid || (r < 0 && errno == ECHILD);
}

void session_power(bool reboot)
{
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/systemctl", "systemctl", reboot ? "reboot" : "poweroff", (char *)NULL);
        _exit(127);
    }
}
