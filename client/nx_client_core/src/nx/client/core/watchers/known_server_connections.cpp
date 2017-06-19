#include "known_server_connections.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/vms/discovery/manager.h>
#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <client/client_message_processor.h>
#include <client_core/client_core_settings.h>

namespace {

template<typename T>
bool trimConnectionsList(QList<T>& list)
{
    constexpr int kMaxItemsToStore = 8;

    if (list.size() <= kMaxItemsToStore)
        return false;

    list.erase(list.begin() + kMaxItemsToStore, list.end());
    return true;
}

} // namespace

namespace nx {
namespace client {
namespace core {
namespace watchers {

KnownServerConnections::KnownServerConnections(QnCommonModule* commonModule, QObject* parent):
    QObject(parent),
    QnCommonModuleAware(commonModule)
{
    const auto moduleManager = commonModule->moduleDiscoveryManager();
    if (!moduleManager)
    {
        NX_ASSERT(moduleManager);
        return;
    }

    m_connections = qnClientCoreSettings->knownServerConnections();
    if (trimConnectionsList(m_connections))
        qnClientCoreSettings->setKnownServerConnections(m_connections);

    using namespace nx::utils;

    for (const auto& connection: m_connections)
        moduleManager->checkEndpoint(connection.url, connection.id);

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionOpened, this,
        [this, moduleManager, commonModule]()
        {
            Connection connection{commonModule->remoteGUID(), commonModule->currentUrl()};

            if (nx::network::SocketGlobals::addressResolver().isCloudHostName(
                connection.url.host()))
            {
                return;
            }

            connection.url = nx::network::url::Builder()
                .setScheme(lit("http"))
                .setHost(connection.url.host())
                .setPort(connection.url.port());

            // Place url to the top of the list.
            m_connections.removeOne(connection);
            m_connections.prepend(connection);
            trimConnectionsList(m_connections);

            moduleManager->checkEndpoint(connection.url, connection.id);
            qnClientCoreSettings->setKnownServerConnections(m_connections);
        }
    );
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(KnownServerConnections::Connection, (json)(eq), (id)(url))

} // namespace watchers
} // namespace core
} // namespace client
} // namespace nx
