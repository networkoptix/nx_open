// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtNetwork/QNetworkProxy>

#include <api/network_proxy_factory.h>
#include <nx/utils/impl_ptr.h>

/**
 * Simplified version of QnNetworkProxyFactory designed to work without QnRouter.
 * It sends all requests via the currently connected server.
 */
class QnSimpleNetworkProxyFactory: public QnNetworkProxyFactory
{
    using base_type = QnNetworkProxyFactory;

public:
    QnSimpleNetworkProxyFactory(QnCommonModule* commonModule);

    virtual QNetworkProxy proxyToResource(
        const QnResourcePtr& resource,
        QnMediaServerResourcePtr* const via = nullptr) const override;

    virtual nx::utils::Url urlToResource(
        const nx::utils::Url& baseUrl,
        const QnResourcePtr& resource,
        const QString& proxyQueryParameterName = QString()) const override;

private:
    class ConnectionHelper;
    nx::utils::ImplPtr<ConnectionHelper> connectionHelper;
};
