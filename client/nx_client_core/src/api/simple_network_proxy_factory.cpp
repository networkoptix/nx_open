#include "simple_network_proxy_factory.h"

#include "core/resource/media_server_resource.h"
#include "core/resource/security_cam_resource.h"

#include "common/common_module.h"

QnSimpleNetworkProxyFactory::QnSimpleNetworkProxyFactory(QnCommonModule* commonModule):
    base_type(commonModule)
{
}

QNetworkProxy QnSimpleNetworkProxyFactory::proxyToResource(const QnResourcePtr &resource,
    QnMediaServerResourcePtr* const via) const
{
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

    nx::utils::Url url = commonModule()->currentUrl();
    if (!url.isValid())
        return QNetworkProxy(QNetworkProxy::NoProxy);

    if (via)
        *via = commonModule()->currentServer();
    return QNetworkProxy(QNetworkProxy::HttpProxy, url.host(), url.port(), url.userName(), url.password());
}

nx::utils::Url QnSimpleNetworkProxyFactory::urlToResource(const nx::utils::Url &baseUrl, const QnResourcePtr &resource,
    const QString &proxyQueryParameterName) const
{
    nx::utils::Url url = base_type::urlToResource(baseUrl, resource, proxyQueryParameterName);

    nx::utils::Url ecUrl = commonModule()->currentUrl();
    if (ecUrl.isValid()) {
        url.setHost(ecUrl.host());
        url.setPort(ecUrl.port());
    }

    return url;
}
