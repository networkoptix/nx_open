#include "cloud_url_validator.h"

#include <api/network_proxy_factory.h>

#include <core/resource/media_server_resource.h>

#include <nx/network/socket_global.h>

namespace nx {
namespace network {

bool isCloudServer(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return false;

    QUrl url = server->getApiUrl();
    if (nx::network::SocketGlobals::addressResolver().isCloudHostName(url.host()))
        return true;

    const QNetworkProxy &proxy = QnNetworkProxyFactory::proxyToResource(server);
    if (proxy.type() == QNetworkProxy::HttpProxy)
        return nx::network::SocketGlobals::addressResolver().isCloudHostName(proxy.hostName());

    return false;
}

} // namespace network
} // namespace nx
