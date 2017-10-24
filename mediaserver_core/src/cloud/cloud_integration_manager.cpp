#include "cloud_integration_manager.h"

#include <nx/network/url/url_builder.h>

#include "media_server/media_server_module.h"
#include "media_server/settings.h"
#include "server/server_globals.h"

CloudIntegrationManager::CloudIntegrationManager(
    QnCommonModule* commonModule,
    AbstractNonceProvider* defaultNonceFetcher)
    :
    cloudManagerGroup(commonModule, defaultNonceFetcher)
{
    const auto cdbEndpoint = qnServerModule->roSettings()->value(
        nx_ms_conf::CDB_ENDPOINT,
        "").toString();
    if (!cdbEndpoint.isEmpty())
    {
        cloudManagerGroup.connectionManager.setCloudDbUrl(
            nx::network::url::Builder().setScheme(nx_http::kUrlSchemeName)
                .setEndpoint(SocketAddress(cdbEndpoint)));
    }

    connect(
        &cloudManagerGroup.connectionManager,
        &AbstractCloudConnectionManager::cloudBindingStatusChanged,
        this,
        &CloudIntegrationManager::onCloudBindingStatusChanged);
}

void CloudIntegrationManager::onCloudBindingStatusChanged(bool isBound)
{
    qnServerModule->roSettings()->setValue(
        QnServer::kIsConnectedToCloudKey,
        isBound ? "yes" : "no");
}
