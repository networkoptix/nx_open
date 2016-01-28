#pragma once

struct QnMigratedSession
{
    QString title;
    QString login;
    QString password;
    QString host;
    int port;

    QnMigratedSession() : port(0) {}
};

typedef QList<QnMigratedSession> QnMigratedSessionList;

QnMigratedSessionList getMigratedSessions(bool *success);

void migrateSettings();
