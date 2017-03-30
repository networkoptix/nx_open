#include "simple_network_proxy_factory.h"

#include "core/resource/media_server_resource.h"
#include "core/resource/security_cam_resource.h"

#include "common/common_module.h"
#include "api/app_server_connection.h"

QNetworkProxy QnSimpleNetworkProxyFactory::proxyToResource(const QnResourcePtr &resource, QnMediaServerResourcePtr* const via) {
    Q_UNUSED(via)

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

    if (commonModule()->remoteGUID() == id)
        return QNetworkProxy(QNetworkProxy::NoProxy);

    QUrl url = commonModule()->currentUrl();
    if (!url.isValid())
        return QNetworkProxy(QNetworkProxy::NoProxy);

    if (via)
        *via = commonModule()->currentServer();
    return QNetworkProxy(QNetworkProxy::HttpProxy, url.host(), url.port(), url.userName(), url.password());
}

QUrl QnSimpleNetworkProxyFactory::urlToResource(const QUrl &baseUrl, const QnResourcePtr &resource, const QString &proxyQueryParameterName) {
    QUrl url = base_type::urlToResource(baseUrl, resource, proxyQueryParameterName);

    QUrl ecUrl = commonModule()->currentUrl();
    if (ecUrl.isValid()) {
        url.setHost(ecUrl.host());
        url.setPort(ecUrl.port());
    }

    return url;
}
