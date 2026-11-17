# PureC-OS-QT-Login

Graphical login screen (display manager) of PureC OS, written in C on libgui
(`PureC-OS-LibGui`) and the video-backend display server. Package and program
name: `login` (`/usr/bin/login`).

- runs as `display-manager.service`, which `graphical.target` wants
- lists the users from `/etc/passwd` (uid 1000+ and root), checks the password
  against `/etc/shadow` with the same code as getty (libpurecrypt); a wrong
  password locks the button for 1.5 s
- starts the session (`/usr/bin/desktop-rs`) as the user with HOME, USER,
  SHELL, PATH and PS1 set; when the session ends the login screen comes back
- live system without `/etc/shadow`: no password needed
- Restart / Power off buttons (systemctl), Esc clears the password,
  Up / Down choose the user

## Build

Inside the PureC-OS tree (libgui and libvb are linked statically):

    make -C programs/PureC-OS-QT-Login            # bin/login/login
    make -C programs/PureC-OS-QT-Login package    # bin/login/login-VERSION-1-x86_64.pkg.tar.gz (+ .sig)
    python3 mk/pkg/publish.py --registry Trusted bin/login/login-VERSION-1-x86_64.pkg.tar.gz

On PureC OS: `pkg -Sy && pkg -S login`. The package conflicts with the old
Qt `qt-login`: remove it first with `pkg -R qt-login`.
