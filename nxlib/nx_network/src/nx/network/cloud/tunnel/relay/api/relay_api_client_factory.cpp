#include <algorithm>

#include "relay_api_client_factory.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_connect_settings.h>
#include <nx/network/socket_global.h>
#include <nx/utils/std/algorithm.h>

#include "relay_api_client_over_http_connect.h"
#include "relay_api_client_over_http_get_post_tunnel.h"
#include "relay_api_client_over_http_upgrade.h"
#include "relay_api_client_over_http_tunnel.h"

namespace nx::cloud::relay::api {

ClientFactory::ClientFactory():
    base_type([this](auto&&... args) { return defaultFactoryFunction(std::move(args)...); })
{
    registerClientType<ClientOverHttpTunnel>();
}

ClientFactory& ClientFactory::instance()
{
    static ClientFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<Client> ClientFactory::defaultFactoryFunction(
    const utils::Url& baseUrl)
{
    using namespace std::placeholders;

    QnMutexLocker lock(&m_mutex);

    auto clientTypeIter = m_clientTypes.begin();

    return clientTypeIter->factoryFunction(
        baseUrl,
        [this, typeId = clientTypeIter->id](ResultCode resultCode)
        {
            processClientFeedback(typeId, resultCode);
        });
}

void ClientFactory::processClientFeedback(int typeId, ResultCode resultCode)
{
    QnMutexLocker lock(&m_mutex);

    const bool typeStillSelected =
        typeId == m_clientTypes.begin()->id;
    if (typeStillSelected && resultCode != ResultCode::ok)
    {
        std::rotate(
            m_clientTypes.begin(),
            std::next(m_clientTypes.begin()),
            m_clientTypes.end());
    }
}

} // namespace nx::cloud::relay::api
