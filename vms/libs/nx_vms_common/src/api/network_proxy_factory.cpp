// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "network_proxy_factory.h"

#include <QtCore/QUrlQuery>

#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/authutil.h>
#include <network/router.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/vms/common/system_context.h>
#include <nx_ec/abstract_ec_connection.h>
#include <utils/common/synctime.h>

nx::utils::Url QnNetworkProxyFactory::urlToResource(
    const nx::utils::Url& baseUrl,
    const QnResourcePtr& resource,
    const QString& proxyQueryParameterName)
{
    QnMediaServerResourcePtr via;
    const QNetworkProxy& proxy = proxyToResource(resource, &via);

    switch (proxy.type())
    {
        case QNetworkProxy::NoProxy:
            break;
        case QNetworkProxy::HttpProxy:
        {
            nx::utils::Url url(baseUrl);
            QUrlQuery query(url.query());
            if (proxyQueryParameterName.isEmpty())
                url.setPath(lit("/proxy/%1%2").arg(resource->getId().toString()).arg(url.path()));
            else
                query.addQueryItem(proxyQueryParameterName, resource->getId().toString());

            url.setQuery(query);
            url.setHost(proxy.hostName());
            url.setPort(proxy.port());

            if (!proxy.user().isEmpty())
            {
                NX_ASSERT(via);
                QUrlQuery urlQuery(url.toQUrl());
                auto nonce = QByteArray::number(qnSyncTime->currentUSecsSinceEpoch(), 16);
                urlQuery.addQueryItem(lit("proxy_auth"),
                    QLatin1String(createHttpQueryAuthParam(proxy.user(),
                        proxy.password(),
                        via->realm(),
                        nx::network::http::Method::get,
                        nonce)));
                url.setQuery(urlQuery);
            }

            return url;
        }
        default:
            NX_ASSERT(false);
    }

    return baseUrl;
}

QNetworkProxy QnNetworkProxyFactory::proxyToResource(
    const QnResourcePtr& resource,
    QnMediaServerResourcePtr* const via)
{
    auto context = resource->systemContext();
    if (!context)
        return QNetworkProxy(QNetworkProxy::NoProxy);

    if (!context->isRoutingEnabled()) //< Cloud cross-system contexts have no routing.
        return QNetworkProxy(QNetworkProxy::NoProxy);

    QnMediaServerResourcePtr server;
    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (camera)
    {
        QnResourcePtr parent = resource->getParentResource();

        while (parent && !parent->hasFlags(Qn::server))
            parent = parent->getParentResource();

        server = parent.dynamicCast<QnMediaServerResource>();
    }
    else
    {
        server = resource.dynamicCast<QnMediaServerResource>();
    }

    const auto& connection = context->messageBusConnection();
    if (server && connection)
    {
        const nx::Uuid id = server->getOriginalGuid();
        QnRoute route = QnRouter::routeTo(id, context);

        if (!route.gatewayId.isNull() || camera)
        {
            if (route.addr.isNull() && !route.reverseConnect)
            {
                NX_WARNING(NX_SCOPE_TAG, "No route to server %1, is connection lost?", id);
                return QNetworkProxy(QNetworkProxy::NoProxy);
            }

            // Reverse connection is not supported for a client.
            if (route.reverseConnect)
                return QNetworkProxy(QNetworkProxy::NoProxy);

            if (via)
                *via = context->resourcePool()->getResourceById<QnMediaServerResource>(route.id);

            // TODO: #sivanov Credentials are actual on the client side only.
            const auto credentials = connection->credentials();
            return QNetworkProxy(
                QNetworkProxy::HttpProxy,
                QString::fromStdString(route.addr.address.toString()),
                route.addr.port,
                QString::fromStdString(credentials.username),
                QString::fromStdString(credentials.authToken.value));
        }
    }

    return QNetworkProxy(QNetworkProxy::NoProxy);
}
