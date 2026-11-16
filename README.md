# PureC-OS-QT-Login

Graphical login screen (display manager) of PureC OS, Qt 6 Widgets on the
video-backend display server.

- runs as `display-manager.service`, which `graphical.target` wants
- lists the users from `/etc/passwd` (uid 1000+ and root), checks the password
  against `/etc/shadow` with the same code as getty (libpurecrypt); a wrong
  password locks the button for 1.5 s
- starts the session (`/usr/bin/desktop-rs`) as the user with HOME, USER,
  SHELL, PATH and PS1 set; when the session ends (Log out in the desktop's
  power menu) the login screen comes back
- live system without `/etc/shadow`: no password needed
- Restart / Power off buttons (systemctl)

## Build

    make -C programs/PureC-OS-QT-Login            # bin/qt-login-build/qt-login
    make -C programs/PureC-OS-QT-Login package    # bin/repo/qt-login-VERSION-1-x86_64.pkg.tar.gz (+ .sig)

On PureC OS: add `qt-login = https://github.com/pablaofficeal/PureC-OS-QT-Login`
under `[trusted]` in `/etc/pkg/pkg.conf`, then `pkg -Sy && pkg -S qt-login` and
`reboot`. userspace 2.0.2 or newer does not start the desktop by itself when
qt-login is installed.
