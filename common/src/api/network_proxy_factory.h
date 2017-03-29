#ifndef QN_NETWORK_PROXY_FACTORY_H
#define QN_NETWORK_PROXY_FACTORY_H

#include <QtNetwork/QNetworkProxy>
#include <nx/utils/singleton.h>

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

// -------------------------------------------------------------------------- //
// QnNetworkProxyFactory
// -------------------------------------------------------------------------- //
/**
 * Note that instance of this class will be used from several threads, and
 * must therefore be thread-safe.
 */
class QnNetworkProxyFactory:
    public QNetworkProxyFactory,
    public QnCommonModuleAware
{
public:
    QnNetworkProxyFactory(QnCommonModule* commonModule);
    virtual ~QnNetworkProxyFactory();

    static QUrl urlToResource(
        const QUrl &baseUrl,
        const QnResourcePtr &resource,
        const QString &proxyQueryParameterName = QString());

    /*!
        \param via In not NULL filled with server, request is to be sent to
    */
    static QNetworkProxy proxyToResource(
        const QnResourcePtr &resource,
        QnMediaServerResourcePtr* const via = nullptr );

protected:
    virtual QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query) override;
};

#endif
