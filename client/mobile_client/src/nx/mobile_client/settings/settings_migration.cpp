#include "settings_migration.h"

#include <boost/range/algorithm/find_if.hpp>

#include <nx/utils/url_builder.h>
#include <mobile_client/mobile_client_settings.h>
#include <client_core/client_core_settings.h>

#include "login_session.h"

namespace nx {
namespace mobile_client {
namespace settings {

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

QnMigratedSessionList getMigratedSessions(bool *success)
{
    if (success)
        *success = false;

    return QnMigratedSessionList();
}

#endif

static void migrateFrom24To25()
{
    if (qnSettings->isSettingsMigrated())
        return;

    bool success = false;
    auto importedSessions = getMigratedSessions(&success);

    if (importedSessions.isEmpty())
    {
        qDebug() << "Settings migration is skipped.";
        return;
    }

    if (!success)
    {
        qDebug() << "Settings migration failed.";
        return;
    }

    QVariantList sessions = qnSettings->savedSessions();

    for (const auto& importedSession: importedSessions)
    {
        QnLoginSession session;
        session.systemName = importedSession.title;
        session.url = nx::utils::UrlBuilder()
            .setScheme(lit("http"))
            .setHost(importedSession.host)
            .setPort(importedSession.port)
            .setUserName(importedSession.login)
            .setPassword(importedSession.password);
        sessions.append(session.toVariant());
    }

    qnSettings->setSavedSessions(sessions);

    qDebug() << "Imported" << importedSessions.size() << "sessions.";

    qnSettings->setSettingsMigrated(true);
}

static void migrateFrom26To30()
{
    const QVariantList sessions = qnSettings->savedSessions();
    if (sessions.isEmpty())
        return;

    auto recentConnections = qnClientCoreSettings->recentLocalConnections();

    for (const QVariant& sessionVariant: sessions)
    {
        const auto session = QnLoginSession::fromVariant(sessionVariant.toMap());

        auto it = boost::find_if(recentConnections,
            [&session](const QnLocalConnectionData& data) { return data.localId == session.id; });

        if (it != recentConnections.end())
            continue;

        const QnLocalConnectionData connectionData(session.systemName, session.id, session.url);
        recentConnections.append(connectionData);
    }

    qnClientCoreSettings->setRecentLocalConnections(recentConnections);
}

void migrateSettings()
{
    migrateFrom24To25();
    migrateFrom26To30();
}

} // namespace settings
} // namespace mobile_client
} // namespace nx
