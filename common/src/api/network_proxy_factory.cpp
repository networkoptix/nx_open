#include "network_proxy_factory.h"

#include <QtCore/QUrlQuery>

#include <api/app_server_connection.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>
#include <network/authenticate_helper.h>
#include <utils/network/router.h>


// -------------------------------------------------------------------------- //
// QnNetworkProxyFactory
// -------------------------------------------------------------------------- //
QnNetworkProxyFactory::QnNetworkProxyFactory() {}
QnNetworkProxyFactory::~QnNetworkProxyFactory() {}

QUrl QnNetworkProxyFactory::urlToResource(const QUrl &baseUrl, const QnResourcePtr &resource, const QString &proxyQueryParameterName) {
    const QNetworkProxy &proxy = proxyToResource(resource);

    switch (proxy.type()) {
    case QNetworkProxy::NoProxy:
        break;
    case QNetworkProxy::HttpProxy: {
        QUrl url(baseUrl);
        QUrlQuery query(url.query());
        if (proxyQueryParameterName.isEmpty())
            url.setPath(lit("/proxy/%1%2").arg(resource->getId().toString()).arg(url.path()));
        else
            query.addQueryItem(proxyQueryParameterName, resource->getId().toString());

        url.setQuery(query);
        url.setHost(proxy.hostName());
        url.setPort(proxy.port());

        if (!proxy.user().isEmpty()) {
            QUrlQuery urlQuery(url);
            urlQuery.addQueryItem(lit("proxy_auth"), QLatin1String(QnAuthHelper::createHttpQueryAuthParam(proxy.user(), proxy.password())));
            url.setQuery(urlQuery);
        }

        return url;
    }
    default:
        Q_ASSERT(0);
    }

    return baseUrl;
}

QList<QNetworkProxy> QnNetworkProxyFactory::queryProxy(const QNetworkProxyQuery &query)
{
    QString urlPath = query.url().path();
    
    if (urlPath.startsWith(QLatin1String("/")))
        urlPath.remove(0, 1);

    if (urlPath.endsWith(QLatin1String("/")))
        urlPath.chop(1);

    if ( urlPath == QLatin1String("api/ping") )
        return QList<QNetworkProxy>() << QNetworkProxy(QNetworkProxy::NoProxy);

    QUrlQuery urlQuery(query.url());

    QnUuid resourceGuid = QnUuid(urlQuery.queryItemValue(lit("x-camera-guid")));
    if (resourceGuid.isNull())
        resourceGuid = QnUuid(urlQuery.queryItemValue(lit("X-server-guid")));

    if (resourceGuid.isNull())
        return QList<QNetworkProxy>() << QNetworkProxy(QNetworkProxy::NoProxy);

	return QList<QNetworkProxy>() << proxyToResource(qnResPool->getIncompatibleResourceById(resourceGuid, true));
}

QNetworkProxy QnNetworkProxyFactory::proxyToResource(const QnResourcePtr &resource) {
    if (!QnRouter::instance())
        return QNetworkProxy(QNetworkProxy::NoProxy);

    QnMediaServerResourcePtr server;
    QnSecurityCamResourcePtr camera = resource.dynamicCast<QnSecurityCamResource>();
    if (camera) {
        QnResourcePtr parent = resource->getParentResource();

        while (parent && !parent->hasFlags(Qn::server))
            parent = parent->getParentResource();

        server = parent.dynamicCast<QnMediaServerResource>();
    } else {
        server = resource.dynamicCast<QnMediaServerResource>();
    }

    if (server) {
        QnUuid id = server->getOriginalGuid();
		if (id.isNull())
			id = server->getId();
        QnRoute route = QnRouter::instance()->routeTo(id);
        if (!route.gatewayId.isNull()) {
            Q_ASSERT(!route.addr.isNull());
            return QNetworkProxy(QNetworkProxy::HttpProxy, route.addr.address.toString(), route.addr.port,
                                    QnAppServerConnectionFactory::url().userName(), QnAppServerConnectionFactory::url().password());
        }
    }

    return QNetworkProxy(QNetworkProxy::NoProxy);
}
