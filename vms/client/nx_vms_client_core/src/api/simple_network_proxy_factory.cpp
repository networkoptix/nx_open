// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "simple_network_proxy_factory.h"

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

using namespace nx::vms::client::core;

class QnSimpleNetworkProxyFactory::ConnectionHelper: public RemoteConnectionAware
{
};

QnSimpleNetworkProxyFactory::QnSimpleNetworkProxyFactory(QnCommonModule* commonModule):
    base_type(commonModule),
    connectionHelper(new ConnectionHelper())
{
}

QNetworkProxy QnSimpleNetworkProxyFactory::proxyToResource(const QnResourcePtr &resource,
    QnMediaServerResourcePtr* const via) const
{
    QnMediaServerResourcePtr server;

    if (const auto camera = resource.dynamicCast<QnSecurityCamResource>())
        server = camera->getParentServer();
    else
        server = resource.dynamicCast<QnMediaServerResource>();

    if (!server)
        return QNetworkProxy(QNetworkProxy::NoProxy);

    QnUuid id = QnUuid::fromStringSafe(server->getProperty("guid"));
    if (id.isNull())
        id = server->getId();

    if (commonModule()->remoteGUID() == id)
        return QNetworkProxy(QNetworkProxy::NoProxy);

    auto remoteConnection = connectionHelper->connection();
    if (!remoteConnection)
        return QNetworkProxy(QNetworkProxy::NoProxy);

    const auto address = connectionHelper->connectionAddress();
    const auto credentials = connectionHelper->connectionCredentials();

    if (via)
        *via = commonModule()->currentServer();

    return QNetworkProxy(
        QNetworkProxy::HttpProxy,
        QString::fromStdString(address.address.toString()),
        address.port,
        QString::fromStdString(credentials.username),
        QString::fromStdString(credentials.authToken.value));
}

nx::utils::Url QnSimpleNetworkProxyFactory::urlToResource(
    const nx::utils::Url& baseUrl,
    const QnResourcePtr& resource,
    const QString& proxyQueryParameterName) const
{
    nx::utils::Url url = base_type::urlToResource(baseUrl, resource, proxyQueryParameterName);

    auto remoteConnection = connectionHelper->connection();
    if (!remoteConnection)
        return url;

    const auto address = connectionHelper->connectionAddress();
    url.setHost(QString::fromStdString(address.address.toString()));
    url.setPort(address.port);

    return url;
}
