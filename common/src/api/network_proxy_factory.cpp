#include "network_proxy_factory.h"

#include <QtCore/QUrlQuery>

#include <api/app_server_connection.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>
#include <network/router.h>
#include <nx/network/http/custom_headers.h>
#include "utils/common/synctime.h"
#include "network/authutil.h"
#include <network/tcp_listener.h>
#include <common/common_module.h>


// -------------------------------------------------------------------------- //
// QnNetworkProxyFactory
// -------------------------------------------------------------------------- //
QnNetworkProxyFactory::QnNetworkProxyFactory(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

QnNetworkProxyFactory::~QnNetworkProxyFactory()
{
}

nx::utils::Url QnNetworkProxyFactory::urlToResource(
    const nx::utils::Url &baseUrl,
    const QnResourcePtr &resource,
    const QString &proxyQueryParameterName) const
{
    QnMediaServerResourcePtr via;
    const QNetworkProxy &proxy = proxyToResource(resource, &via);

    switch (proxy.type()) {
    case QNetworkProxy::NoProxy:
        break;
    case QNetworkProxy::HttpProxy: {
        nx::utils::Url url(baseUrl);
        QUrlQuery query(url.query());
        if (proxyQueryParameterName.isEmpty())
            url.setPath(lit("/proxy/%1%2").arg(resource->getId().toString()).arg(url.path()));
        else
            query.addQueryItem(proxyQueryParameterName, resource->getId().toString());

        url.setQuery(query);
        url.setHost(proxy.hostName());
        url.setPort(proxy.port());

        if (!proxy.user().isEmpty()) {
            NX_ASSERT( via );
            QUrlQuery urlQuery(url.toQUrl());
            auto nonce = QByteArray::number( qnSyncTime->currentUSecsSinceEpoch(), 16 );
            urlQuery.addQueryItem(
                lit("proxy_auth"),
                QLatin1String(createHttpQueryAuthParam(
                    proxy.user(),
                    proxy.password(),
                    via->realm(),
                    nx_http::Method::get,
                    nonce)));
            url.setQuery(urlQuery);
        }

        return url;
    }
    default:
        NX_ASSERT(0);
    }

    return baseUrl;
}

QList<QNetworkProxy> QnNetworkProxyFactory::queryProxy(const QNetworkProxyQuery &query)
{
    QString urlPath = QnTcpListener::normalizedPath(query.url().path());

    if ( urlPath == QLatin1String("api/ping") )
        return QList<QNetworkProxy>() << QNetworkProxy(QNetworkProxy::NoProxy);

    QUrlQuery urlQuery(query.url());

    QnUuid resourceGuid = QnUuid(urlQuery.queryItemValue(QString::fromLatin1(Qn::CAMERA_GUID_HEADER_NAME)));
    if (resourceGuid.isNull())
        resourceGuid = QnUuid(urlQuery.queryItemValue(QString::fromLatin1(Qn::SERVER_GUID_HEADER_NAME)));

    if (resourceGuid.isNull())
        return QList<QNetworkProxy>() << QNetworkProxy(QNetworkProxy::NoProxy);

    return QList<QNetworkProxy>() << proxyToResource(
        commonModule()->resourcePool()->getIncompatibleServerById(resourceGuid, true));
}

QNetworkProxy QnNetworkProxyFactory::proxyToResource(
    const QnResourcePtr &resource,
    QnMediaServerResourcePtr* const via ) const
{
    if (!commonModule()->router())
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

    const auto& connection = commonModule()->ec2Connection();
    if (server && connection)
    {
        QnUuid id = server->getOriginalGuid();
        QnRoute route = commonModule()->router()->routeTo(id);
        if (!route.gatewayId.isNull() || camera) {
            NX_ASSERT(!route.addr.isNull() || route.reverseConnect);

            if (route.reverseConnect)
                // reverse connection is not supported for a client
                return QNetworkProxy(QNetworkProxy::NoProxy);

            if( via )
                *via = server->resourcePool()->getResourceById<QnMediaServerResource>( route.id );

            const auto& url = connection->connectionInfo().ecUrl;
            return QNetworkProxy(QNetworkProxy::HttpProxy,
                                 route.addr.address.toString(), route.addr.port,
                                 url.userName(), url.password());
        }
    }

    return QNetworkProxy(QNetworkProxy::NoProxy);
}
