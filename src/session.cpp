#include "session.h"

#include <QtCore/QFile>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" {
#include "accounts.h"
}

static const char *SessionProgram = "/usr/bin/desktop-rs";

namespace session {

QList<UserEntry> users()
{
    QList<UserEntry> list;
    QFile file(QStringLiteral("/etc/passwd"));
    if (!file.open(QIODevice::ReadOnly))
        return list;
    UserEntry root;
    bool haveRoot = false;
    for (const QByteArray &raw : file.readAll().split('\n')) {
        QList<QByteArray> f = raw.split(':');
        if (f.size() < 7)
            continue;
        bool ok = false;
        uint uid = f[2].toUInt(&ok);
        if (!ok || f[6].contains("nologin") || f[6].contains("false"))
            continue;
        UserEntry u;
        u.name = QString::fromUtf8(f[0]);
        u.fullName = QString::fromUtf8(f[4]).section(QLatin1Char(','), 0, 0);
        u.uid = uid;
        if (uid == 0) {
            root = u;
            haveRoot = true;
        } else if (uid >= 1000 && uid < 60000) {
            list.append(u);
        }
    }
    if (haveRoot)
        list.append(root);
    return list;
}

bool liveSystem()
{
    return access("/etc/shadow", F_OK) != 0;
}

QString hostName()
{
    QFile file(QStringLiteral("/etc/hostname"));
    if (file.open(QIODevice::ReadOnly)) {
        QString name = QString::fromUtf8(file.readAll()).trimmed();
        if (!name.isEmpty())
            return name;
    }
    return QStringLiteral("purec");
}

bool authenticate(const QString &name, const QString &password, QString *error)
{
    if (name.isEmpty()) {
        *error = QStringLiteral("Choose a user");
        return false;
    }
    if (liveSystem())
        return true;
    QByteArray n = name.toUtf8(), p = password.toUtf8();
    struct user_info info;
    bool ok = account_login(n.constData(), p.constData(), &info);
    memset(p.data(), 0, size_t(p.size()));
    if (!ok)
        *error = QStringLiteral("Wrong user name or password");
    return ok;
}

static void addEnv(char **envp, int *n, char storage[][200], const char *key, const char *value)
{
    snprintf(storage[*n], 200, "%s=%s", key, value);
    envp[*n] = storage[*n];
    (*n)++;
}

int start(const QString &name, QString *error)
{
    struct user_info who;
    QByteArray n = name.toUtf8();
    if (pc_user_lookup(n.constData(), 0, &who) != 0) {
        *error = QStringLiteral("Unknown user %1").arg(name);
        return -1;
    }
    if (access(SessionProgram, X_OK) != 0) {
        *error = QStringLiteral("The desktop is not installed (pkg -S userspace)");
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        *error = QStringLiteral("Cannot start the session");
        return -1;
    }
    if (pid > 0)
        return int(pid);
    setsid();
    for (int s = 1; s < 32; ++s)
        signal(s, SIG_DFL);
    if (pc_setgid(who.gid) != 0 || pc_setuid(who.uid) != 0)
        _exit(126);
    const char *home = who.home[0] ? who.home : "/";
    if (chdir(home) != 0) {
        home = "/";
        (void)chdir("/");
    }
    const char *shell = who.shell[0] && access(who.shell, X_OK) == 0 ? who.shell : "/bin/sh";
    static char storage[12][200];
    char *envp[13];
    int k = 0;
    addEnv(envp, &k, storage, "HOME", home);
    addEnv(envp, &k, storage, "USER", who.name);
    addEnv(envp, &k, storage, "LOGNAME", who.name);
    addEnv(envp, &k, storage, "SHELL", shell);
    addEnv(envp, &k, storage, "PATH", who.uid == 0 ? "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"
                                                    : "/usr/local/bin:/usr/bin:/bin");
    addEnv(envp, &k, storage, "TERM", "xterm-256color");
    addEnv(envp, &k, storage, "PS1", who.uid == 0
               ? "\\[\\e[1;31m\\]\\u@\\h\\[\\e[0m\\]:\\[\\e[1;34m\\]\\w\\[\\e[0m\\]# "
               : "\\[\\e[1;32m\\]\\u@\\h\\[\\e[0m\\]:\\[\\e[1;34m\\]\\w\\[\\e[0m\\]$ ");
    addEnv(envp, &k, storage, "XDG_SESSION_TYPE", "videod");
    envp[k] = nullptr;
    char *argv[] = {(char *)"desktop-rs", nullptr};
    execve(SessionProgram, argv, envp);
    _exit(127);
}

bool finished(int pid)
{
    int status = 0;
    pid_t r = waitpid(pid_t(pid), &status, WNOHANG);
    return r == pid || (r < 0 && errno == ECHILD);
}

void power(bool reboot)
{
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/systemctl", "systemctl", reboot ? "reboot" : "poweroff", (char *)nullptr);
        _exit(127);
    }
}

}
