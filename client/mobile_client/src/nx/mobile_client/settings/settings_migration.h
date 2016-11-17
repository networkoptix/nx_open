#pragma once

#include <QtCore/QList>
#include <QtCore/QString>

namespace nx {
namespace mobile_client {
namespace settings {

struct QnMigratedSession
{
    QString title;
    QString login;
    QString password;
    QString host;
    int port = 0;
};

typedef QList<QnMigratedSession> QnMigratedSessionList;

QnMigratedSessionList getMigratedSessions(bool* success);

void migrateSettings();

} // namespace settings
} // namespace mobile_client
} // namespace nx

