#include "migration_helper.h"

#include <functional>

#include <common/common_module.h>

#include <nx/network/url/url_parse_helper.h>

#include <nx/vms/discovery/manager.h>
#include <network/system_helpers.h>
#include <client_core/client_core_settings.h>
#include <mobile_client/mobile_client_settings.h>
#include <helpers/system_helpers.h>

#include "login_session.h"

namespace nx {
namespace mobile_client {
namespace settings {

static QnUuid savedSessionId(const QVariant& sessionVariant)
{
    return QnLoginSession::fromVariant(sessionVariant.toMap()).id;
}

class SessionsMigrationHelperPrivate: public QObject, public QnConnectionContextAware
{
public:
    void at_moduleFound(
        nx::vms::discovery::ModuleEndpoint moduleData)
    {
        using namespace nx::client::core::helpers;
        using nx::client::core::LocalConnectionData;

        const auto systemId = helpers::getLocalSystemId(moduleData);
        auto recentConnections = qnClientCoreSettings->recentLocalConnections();
        auto authenticationData = qnClientCoreSettings->systemAuthenticationData();
        auto savedSessions = qnSettings->savedSessions();
        bool recentConnectionsChanged = false;

        auto findConnection =
            [&recentConnections](
                std::function<bool(const QnUuid&, const LocalConnectionData&)> check)
            {
                auto it = recentConnections.begin();
                for (; it != recentConnections.end(); ++it)
                {
                    if (check(it.key(), it.value()))
                        break;
                }
                return it;
            };

        while (!migratedSessionIds.empty())
        {
            // There could be multiple sessions with the same URL and even same credentials.

            auto migratedIt = findConnection(
                [this, &moduleData](
                    const QnUuid& localId, const LocalConnectionData& data)
                {
                    if (!migratedSessionIds.contains(localId))
                        return false;

                    for (const auto& url: data.urls)
                    {
                        if (moduleData.endpoint == nx::network::url::getEndpoint(url))
                            return true;
                    }

                    return false;
                });

            if (migratedIt == recentConnections.end())
                break;

            const auto fakeId = migratedIt.key();
            const auto connectionData = migratedIt.value();
            const auto credentialsList = authenticationData.take(fakeId);

            recentConnections.erase(migratedIt);
            removeConnection(fakeId);

            savedSessions.erase(
                std::find_if(savedSessions.begin(), savedSessions.end(),
                    [&fakeId](const QVariant& sessionVariant)
                    {
                        return fakeId == savedSessionId(sessionVariant);
                    }));

            for (const auto& url: connectionData.urls)
                storeConnection(systemId, connectionData.systemName, url);

            for (const auto& credentials: credentialsList)
            {
                if (getCredentials(systemId, credentials.user).isEmpty())
                    storeCredentials(systemId, credentials);
            }

            recentConnectionsChanged = true;
        }

        if (recentConnectionsChanged)
            qnSettings->setSavedSessions(savedSessions);
    }

public:
    QSet<QnUuid> migratedSessionIds;
};

SessionsMigrationHelper::SessionsMigrationHelper(QObject* parent):
    QObject(parent),
    d_ptr(new SessionsMigrationHelperPrivate())
{
    Q_D(SessionsMigrationHelper);

    const auto moduleManager = commonModule()->moduleDiscoveryManager();
    NX_ASSERT(moduleManager);
    for (const auto& sessionVariant: qnSettings->savedSessions())
    {
        const auto& session = QnLoginSession::fromVariant(sessionVariant.toMap());
        d->migratedSessionIds.insert(session.id);
        moduleManager->checkEndpoint(session.url);
    }

    for (const auto& module: moduleManager->getAll())
        d->at_moduleFound(module);

    connect(moduleManager, &nx::vms::discovery::Manager::found,
        d, &SessionsMigrationHelperPrivate::at_moduleFound);
}

SessionsMigrationHelper::~SessionsMigrationHelper()
{
}

} // namespace settings
} // namespace mobile_client
} // namespace nx
