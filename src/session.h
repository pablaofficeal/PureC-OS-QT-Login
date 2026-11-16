#pragma once

#include <QtCore/QList>
#include <QtCore/QString>

struct UserEntry {
    QString name;
    QString fullName;
    uint uid = 0;
};

namespace session {

QList<UserEntry> users();
bool liveSystem();
QString hostName();
bool authenticate(const QString &name, const QString &password, QString *error);
int start(const QString &name, QString *error);
bool finished(int pid);
void power(bool reboot);

}
