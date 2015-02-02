#ifndef QN_NETWORK_PROXY_FACTORY_H
#define QN_NETWORK_PROXY_FACTORY_H

#include <QtNetwork/QNetworkProxy>
#include <utils/common/singleton.h>

#include <core/resource/resource_fwd.h>

// -------------------------------------------------------------------------- //
// QnNetworkProxyFactory
// -------------------------------------------------------------------------- //
/**
 * Note that instance of this class will be used from several threads, and
 * must therefore be thread-safe.
 */
class QnNetworkProxyFactory : public QNetworkProxyFactory, public Singleton<QnNetworkProxyFactory> {
public:
    enum WhereToPlaceProxyCredentials {
        noCredentials,
        placeCredentialsToUrl,
        placeCredentialsToHttpMessage
    };

    QnNetworkProxyFactory();
    virtual ~QnNetworkProxyFactory();

    QUrl urlToResource(const QUrl &baseUrl, const QnResourcePtr &resource,
                       WhereToPlaceProxyCredentials credentialsBehavior = placeCredentialsToUrl);

    virtual QNetworkProxy proxyToResource(const QnResourcePtr &resource);

protected:
    virtual QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query) override;
};

#endif
