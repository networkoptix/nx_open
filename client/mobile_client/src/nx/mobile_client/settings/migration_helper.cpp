#include "migration_helper.h"

#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/algorithm/remove_if.hpp>

#include <nx/network/url/url_parse_helper.h>

#include <network/module_finder.h>
#include <network/direct_module_finder_helper.h>
#include <network/system_helpers.h>
#include <client_core/client_core_settings.h>
#include <mobile_client/mobile_client_settings.h>

#include "login_session.h"

namespace nx {
namespace mobile_client {
namespace settings {

static QnUuid savedSessionId(const QVariant& sessionVariant)
{
    return QnLoginSession::fromVariant(sessionVariant.toMap()).id;
}

class SessionsMigrationHelperPrivate: public QObject
{
public:
    void at_moduleFinder_moduleAddressFound(
        const QnModuleInformation &moduleInformation, const SocketAddress &address)
    {
        const auto systemId = helpers::getLocalSystemId(moduleInformation);
        auto recentConnections = qnClientCoreSettings->recentLocalConnections();
        auto savedSessions = qnSettings->savedSessions();
        bool recentConnectionsChanged = false;

        while (!migratedSessionIds.empty())
        {
            // There could be multiple sessions with the same URL and even same credentials.
            auto migratedIt = boost::find_if(recentConnections,
                [this, &moduleInformation, &address](const QnLocalConnectionData& connectionData)
                {
                    return address == nx::network::url::getEndpoint(connectionData.url)
                        && moduleInformation.systemName == connectionData.systemName
                        && migratedSessionIds.contains(connectionData.localId);
                });

            if (migratedIt == recentConnections.end())
                break;

            savedSessions.erase(
                boost::remove_if(
                    savedSessions,
                    [migratedIt](const QVariant& sessionVariant)
                    {
                        return migratedIt->localId == savedSessionId(sessionVariant);
                    }),
                savedSessions.end());

            auto existingIt = boost::find_if(recentConnections,
                 [this, &moduleInformation, &address, migratedIt]
                    (const QnLocalConnectionData& connectionData)
                 {
                     return address == nx::network::url::getEndpoint(connectionData.url)
                         && moduleInformation.systemName == connectionData.systemName
                         && migratedIt->url.userName() == connectionData.url.userName()
                         && !migratedSessionIds.contains(connectionData.localId);
                 });
            migratedSessionIds.remove(migratedIt->localId);
            qnModuleFinder->directModuleFinderHelper()->removeForcedUrl(this, address.toUrl());

            if (existingIt == recentConnections.end())
                migratedIt->localId = systemId;
            else
                recentConnections.erase(migratedIt);

            recentConnectionsChanged = true;
        }

        if (recentConnectionsChanged)
        {
            qnClientCoreSettings->setRecentLocalConnections(recentConnections);
            qnSettings->setSavedSessions(savedSessions);
        }
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
