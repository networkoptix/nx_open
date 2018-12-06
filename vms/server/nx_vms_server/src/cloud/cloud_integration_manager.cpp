#include "cloud_integration_manager.h"

#include <nx/network/url/url_builder.h>

#include "media_server/media_server_module.h"
#include "media_server/settings.h"
#include <nx/vms/auth/generic_user_data_provider.h>
#include <nx/utils/timer_manager.h>
#include "server/server_globals.h"

CloudIntegrationManager::CloudIntegrationManager(
    QnMediaServerModule* serverModule,
    ::ec2::AbstractTransactionMessageBus* transactionMessageBus,
    nx::vms::auth::AbstractNonceProvider* defaultNonceFetcher)
    :
    nx::vms::server::ServerModuleAware(serverModule),
    m_cloudConnector(transactionMessageBus),
    m_cloudManagerGroup(
        serverModule->commonModule(),
        defaultNonceFetcher,
        &m_cloudConnector,
        std::make_unique<GenericUserDataProvider>(serverModule->commonModule()),
        nx::utils::parseTimerDuration(settings().delayBeforeSettingMasterFlag()))
{
    const auto cdbEndpoint = settings().cdbEndpoint();
    if (!cdbEndpoint.isEmpty())
    {
        m_cloudManagerGroup.setCloudDbUrl(
            nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
                .setEndpoint(nx::network::SocketAddress(cdbEndpoint)));
    }

    connect(
        &m_cloudManagerGroup.connectionManager,
        &nx::vms::cloud_integration::AbstractCloudConnectionManager::cloudBindingStatusChanged,
        this,
        &CloudIntegrationManager::onCloudBindingStatusChanged);
}

nx::vms::cloud_integration::CloudManagerGroup& CloudIntegrationManager::cloudManagerGroup()
{
    return m_cloudManagerGroup;
}

void CloudIntegrationManager::onCloudBindingStatusChanged(bool isBound)
{
    serverModule()->roSettings()->setValue(
        QnServer::kIsConnectedToCloudKey,
        isBound ? "yes" : "no");
}
