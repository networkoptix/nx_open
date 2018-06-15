#include "relay_api_client_factory.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_connect_settings.h>
#include <nx/network/socket_global.h>

#include "relay_api_client_over_http_connect.h"
#include "relay_api_client_over_http_upgrade.h"

namespace nx::cloud::relay::api {

ClientFactory::ClientFactory():
    base_type(std::bind(&ClientFactory::defaultFactoryFunction, this,
        std::placeholders::_1))
{
    registerClientType<ClientImplUsingHttpConnect>(0);
    registerClientType<ClientOverHttpUpgrade>(1);
}

ClientFactory& ClientFactory::instance()
{
    static ClientFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<Client> ClientFactory::defaultFactoryFunction(
    const utils::Url& baseUrl)
{
    return m_clientTypesByPriority.begin()->second(baseUrl);
}

} // namespace nx::cloud::relay::api
