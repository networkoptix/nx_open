#include "settings_migration.h"

#include <boost/range/algorithm/find_if.hpp>

#include <nx/utils/url_builder.h>
#include <nx/utils/log/log.h>
#include <mobile_client/mobile_client_settings.h>
#include <client_core/client_core_settings.h>

#include "login_session.h"

namespace nx {
namespace mobile_client {
namespace settings {

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

QnMigratedSessionList getMigratedSessions(bool* success)
{
    if (success)
        *success = true;

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
        NX_LOG("Settings migration is skipped.", cl_logDEBUG1);
        return;
    }

    if (!success)
    {
        NX_LOG("Settings migration failed.", cl_logERROR);
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

    NX_LOG(lit("Imported %1 sessions.").arg(importedSessions.size()), cl_logINFO);

    qnSettings->setSettingsMigrated(true);
}

static void migrateFrom26To30()
{
    const QVariantList sessions = qnSettings->savedSessions();
    if (sessions.isEmpty())
        return;

    auto recentConnections = qnClientCoreSettings->recentLocalConnections();
    auto weights = qnClientCoreSettings->localSystemWeightsData();

    const auto dateTime = QDateTime::currentMSecsSinceEpoch();

    for (const QVariant& sessionVariant: sessions)
    {
        const auto session = QnLoginSession::fromVariant(sessionVariant.toMap());

        {
            auto it = boost::find_if(recentConnections,
                [&session](const QnLocalConnectionData& data)
                {
                    return data.localId == session.id
                            && data.url.userName().compare(
                                session.url.userName(), Qt::CaseInsensitive) == 0;
                });

            if (it != recentConnections.end())
                continue;
        }

        const QnLocalConnectionData connectionData(session.systemName, session.id, session.url);
        recentConnections.append(connectionData);

        {
            auto it = boost::find_if(weights,
                [session](const QnWeightData& data) { return data.localId == session.id; });

            if (it == weights.end())
                weights.append(QnWeightData{session.id, 1.0, dateTime, true});
        }
    }

    qnClientCoreSettings->setRecentLocalConnections(recentConnections);
    qnClientCoreSettings->setLocalSystemWeightsData(weights);
    qnClientCoreSettings->save();
    qnSettings->setSavedSessions(QVariantList());
    qnSettings->save();
}

void migrateSettings()
{
    migrateFrom24To25();
    migrateFrom26To30();
}

} // namespace settings
} // namespace mobile_client
} // namespace nx
