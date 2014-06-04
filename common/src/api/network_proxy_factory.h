#ifndef QN_NETWORK_PROXY_FACTORY_H
#define QN_NETWORK_PROXY_FACTORY_H

#include <QtNetwork/qnetworkproxy.h>
#include <QtCore/QPointer>

#include <core/resource/resource_fwd.h>


// -------------------------------------------------------------------------- //
// QnNetworkProxyFactory
// -------------------------------------------------------------------------- //
/**
 * Note that instance of this class will be used from several threads, and
 * must therefore be thread-safe.
 */
class QnNetworkProxyFactory: public QObject, public QNetworkProxyFactory {
public:
    QnNetworkProxyFactory();
    virtual ~QnNetworkProxyFactory();

    //!Informs that requests to host \a targetHost MUST be proxied vi \a proxyHost:\a proxyPort with proxy credentials \a userName and \a password
    /*!
        \note Replaces any existing binding for \a targetHost
    */
    void addToProxyList(
        const QString& targetHost,
        const QString& proxyHost,
        quint16 proxyPort,
        const QString& userName = QString(),
        const QString& password = QString() );
    /*!
        \note This method is required since different cameras can have same ip address (if they belong to different networks)
        \note This class does not store referenece to \a resource. It is used in this method only
        TODO #ak if custom http header could be passed to queryProxy, this method would not be required. May be there is some other way to pass resource id to queryProxy method?
    */
    void bindHostToResource(
        const QString& targetHost,
        QnResourcePtr resource );
    //!Removes proxy added by \a QnNetworkProxyFactory::addToProxyList or \a QnNetworkProxyFactory::bindHostToResource
    void removeFromProxyList( const QString& targetHost );

    void clearProxyList();

    static QnNetworkProxyFactory* instance();

protected:
    virtual QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query = QNetworkProxyQuery()) override;

private:
    QMutex m_mutex;
    QMap<QString, QNetworkProxy> m_proxyInfo;
};

#endif
