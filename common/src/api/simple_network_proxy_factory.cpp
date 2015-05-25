#include "simple_network_proxy_factory.h"

#include "core/resource/media_server_resource.h"
#include "core/resource/security_cam_resource.h"

#include "common/common_module.h"
#include "api/app_server_connection.h"

QNetworkProxy QnSimpleNetworkProxyFactory::proxyToResource(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server;

    if (resource.dynamicCast<QnSecurityCamResource>())
        server = resource->getParentResource().dynamicCast<QnMediaServerResource>();
    else
        server = resource.dynamicCast<QnMediaServerResource>();

    if (!server)
        return QNetworkProxy(QNetworkProxy::NoProxy);

    QnUuid id = QnUuid::fromStringSafe(server->getProperty(lit("guid")));
    if (id.isNull())
        id = server->getId();

    if (qnCommon->remoteGUID() == id)
        return QNetworkProxy(QNetworkProxy::NoProxy);

    QUrl url = QnAppServerConnectionFactory::url();
    if (!url.isValid())
        return QNetworkProxy(QNetworkProxy::NoProxy);

    return QNetworkProxy(QNetworkProxy::HttpProxy, url.host(), url.port(), url.userName(), url.password());
}
