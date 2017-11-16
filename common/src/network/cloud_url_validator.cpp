#include "cloud_url_validator.h"

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/socket_global.h>

#include <api/network_proxy_factory.h>
#include <core/resource/media_server_resource.h>

namespace nx {
namespace network {

bool isCloudServer(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return false;

    nx::utils::Url url = server->getApiUrl();
    if (nx::network::SocketGlobals::addressResolver().isCloudHostName(url.host()))
        return true;

    QnNetworkProxyFactory proxyFactory(server->commonModule());
    const QNetworkProxy &proxy = proxyFactory.proxyToResource(server);
    if (proxy.type() == QNetworkProxy::HttpProxy)
        return nx::network::SocketGlobals::addressResolver().isCloudHostName(proxy.hostName());

    return false;
}

} // namespace network
} // namespace nx
