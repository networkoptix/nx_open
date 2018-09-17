#include "client_factory.h"

#include <algorithm>

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_connect_settings.h>
#include <nx/network/socket_global.h>
#include <nx/utils/std/algorithm.h>

#include "get_post_tunnel_client.h"

namespace nx::network::http::tunneling::detail {

ClientFactory::ClientFactory():
    base_type(std::bind(&ClientFactory::defaultFactoryFunction, this,
        std::placeholders::_1))
{
    registerClientType<GetPostTunnelClient>();
}

void ClientFactory::clear()
{
    m_clientTypes.clear();
}

ClientFactory& ClientFactory::instance()
{
    static ClientFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<BasicTunnelClient> ClientFactory::defaultFactoryFunction(
    const utils::Url& baseUrl)
{
    using namespace std::placeholders;

    QnMutexLocker lock(&m_mutex);

    auto clientTypeIter = m_clientTypes.begin();

    return clientTypeIter->factoryFunction(
        baseUrl,
        [this, typeId = clientTypeIter->id](bool success)
        {
            processClientFeedback(typeId, success);
        });
}

void ClientFactory::processClientFeedback(int typeId, bool success)
{
    QnMutexLocker lock(&m_mutex);

    const bool typeStillSelected = typeId == m_clientTypes.begin()->id;
    if (typeStillSelected && !success)
    {
        std::rotate(
            m_clientTypes.begin(),
            std::next(m_clientTypes.begin()),
            m_clientTypes.end());
    }
}

} // namespace nx::network::http::tunneling::detail
