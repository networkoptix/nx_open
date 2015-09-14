#ifndef QNSIMPLENETWORKPROXYFACTORY_H
#define QNSIMPLENETWORKPROXYFACTORY_H

#include <api/network_proxy_factory.h>

/**
 * This is a simplified version of QnNetworkProxyFactory designed to work without QnRouter.
 * It sends all requests via the currently connected server.
 */

class QnSimpleNetworkProxyFactory : public QnNetworkProxyFactory {
    typedef QnNetworkProxyFactory base_type;
public:
    virtual QNetworkProxy proxyToResource(const QnResourcePtr &resource) override;
    virtual QUrl urlToResource(const QUrl &baseUrl, const QnResourcePtr &resource, const QString &proxyQueryParameterName = QString()) override;
};

#endif // QNSIMPLENETWORKPROXYFACTORY_H
