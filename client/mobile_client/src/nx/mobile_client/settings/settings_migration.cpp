#include "settings_migration.h"

#include <boost/algorithm/cxx11/any_of.hpp>

#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>
#include <mobile_client/mobile_client_settings.h>
#include <client_core/client_core_settings.h>
#include <helpers/url_helper.h>
#include <helpers/system_helpers.h>

#include "login_session.h"

using boost::algorithm::any_of;

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
        session.url = nx::network::url::Builder()
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
    using namespace client::core;
    using namespace client::core::helpers;

    const QVariantList sessions = qnSettings->savedSessions();
    if (sessions.isEmpty())
        return;

    auto weights = qnClientCoreSettings->localSystemWeightsData();
    auto knownServerUrls = qnClientCoreSettings->knownServerUrls();

    const auto dateTime = QDateTime::currentMSecsSinceEpoch();

    for (const auto& sessionVariant: sessions)
    {
        const auto session = QnLoginSession::fromVariant(sessionVariant.toMap());

        if (qnClientCoreSettings->recentLocalConnections().contains(session.id))
            continue;

        storeConnection(session.id, session.systemName, session.url);
        storeCredentials(
            session.id, QnEncodedCredentials(session.url.userName(), session.url.password()));
        weights.append(WeightData{session.id, 1.0, dateTime, true});
    }

    qnClientCoreSettings->setLocalSystemWeightsData(weights);
    qnClientCoreSettings->setKnownServerUrls(knownServerUrls);
    qnClientCoreSettings->save();
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
