#include "settings_migration.h"

#include <context/login_session.h>
#include <mobile_client/mobile_client_settings.h>

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

QnMigratedSessionList getMigratedSessions(bool *success)
{
    if (success)
        *success = false;

    return QnMigratedSessionList();
}

#endif

void migrateSettings()
{
    // TODO: #dklychkov #3.0 Migrate settings from 2.6

    if (qnSettings->isSettingsMigrated())
        return;

    bool success = false;
    auto importedSessions = getMigratedSessions(&success);

    if (importedSessions.isEmpty())
    {
        qDebug() << "Settings migration is skipped.";
        return;
    }

    QVariantList sessions = qnSettings->savedSessions();

    for (const auto &importedSession : importedSessions)
    {
        QnLoginSession session;
        session.systemName = importedSession.title;
        session.address = importedSession.host;
        session.port = importedSession.port;
        session.user = importedSession.login;
        session.password = importedSession.password;

        sessions.append(session.toVariant());
    }

    qnSettings->setSavedSessions(sessions);

    qDebug() << "Imported" << importedSessions.size() << "sessions.";

    if (success)
        qnSettings->setSettingsMigrated(true);
    else
        qDebug() << "Settings migration failed.";
}
