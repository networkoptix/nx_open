#include "migration_helper.h"

#include <functional>

#include <common/common_module.h>

#include <nx/network/url/url_parse_helper.h>

#include <network/module_finder.h>
#include <network/direct_module_finder.h>
#include <network/direct_module_finder_helper.h>
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
    void at_moduleFinder_moduleAddressFound(
        const QnModuleInformation &moduleInformation, const SocketAddress &address)
    {
        using namespace nx::client::core::helpers;
        using nx::client::core::LocalConnectionData;

        const auto systemId = helpers::getLocalSystemId(moduleInformation);
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
                [this, &moduleInformation, &address](
                    const QnUuid& localId, const LocalConnectionData& data)
                {
                    if (!migratedSessionIds.contains(localId))
                        return false;

                    for (const auto& url: data.urls)
                    {
                        if (address == nx::network::url::getEndpoint(url))
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

            for (const auto& url: connectionData.urls)
                qnModuleFinder->directModuleFinderHelper()->removeForcedUrl(this, url);

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

    NX_ASSERT(qnModuleFinder);

    for (const auto& sessionVariant: qnSettings->savedSessions())
    {
        const auto& session = QnLoginSession::fromVariant(sessionVariant.toMap());
        d->migratedSessionIds.insert(session.id);
        qnModuleFinder->directModuleFinderHelper()->addForcedUrl(d, session.url);
        qnModuleFinder->directModuleFinder()->checkUrl(session.url);
    }

    for (const auto& moduleInformation: qnModuleFinder->foundModules())
    {
        for (const auto& address: qnModuleFinder->moduleAddresses(moduleInformation.id))
            d->at_moduleFinder_moduleAddressFound(moduleInformation, address);
    }

    connect(qnModuleFinder, &QnModuleFinder::moduleAddressFound,
        d, &SessionsMigrationHelperPrivate::at_moduleFinder_moduleAddressFound);
}

SessionsMigrationHelper::~SessionsMigrationHelper()
{
}

} // namespace settings
} // namespace mobile_client
} // namespace nx
