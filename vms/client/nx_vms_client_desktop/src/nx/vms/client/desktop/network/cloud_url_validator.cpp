// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_url_validator.h"

#include <api/network_proxy_factory.h>
#include <client_core/client_core_module.h>
#include <core/resource/media_server_resource.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>

namespace nx::vms::client::desktop {

bool isCloudServer(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return false;

    nx::utils::Url url = server->getApiUrl();
    if (nx::network::SocketGlobals::addressResolver().isCloudHostname(url.host()))
        return true;

    const QNetworkProxy &proxy = QnNetworkProxyFactory::proxyToResource(server);
    if (proxy.type() == QNetworkProxy::HttpProxy)
        return nx::network::SocketGlobals::addressResolver().isCloudHostname(proxy.hostName());

    return false;
}

} // namespace nx::vms::client::desktop
