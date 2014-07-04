#ifndef QN_NETWORK_PROXY_FACTORY_H
#define QN_NETWORK_PROXY_FACTORY_H

#include <QtCore/QPointer>
#include <QtCore/QMutex>
#include <QtNetwork/QNetworkProxyFactory>

#include <core/resource/resource_fwd.h>


// -------------------------------------------------------------------------- //
// QnNetworkProxyFactory
// -------------------------------------------------------------------------- //
/**
 * Note that instance of this class will be used from several threads, and
 * must therefore be thread-safe.
 */
class QnNetworkProxyFactory
:
    public QObject,
    public QNetworkProxyFactory
{
public:
    enum WhereToPlaceProxyCredentials
    {
        noCredentials,
        placeCredentialsToUrl,
        placeCredentialsToHttpMessage
    };

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
        const QnResourcePtr& resource );
    //!Removes proxy added by \a QnNetworkProxyFactory::addToProxyList or \a QnNetworkProxyFactory::bindHostToResource
    void removeFromProxyList( const QString& targetHost );
    void clearProxyList();

    //!
    /*!
        Can modify host, port in \a requestUrl. If proxying is used, can add prefix to \a url's path and add some parameter to query
        \note \a requestUrl MUST have valid host, port, path, user password. These can be changed if and only if proxying is required
    */
    bool fillUrlWithRouteToResource(
        const QnResourcePtr& targetResource,
        QUrl* const requestUrl,
        WhereToPlaceProxyCredentials credentialsBehavour );

    static QnNetworkProxyFactory* instance();

protected:
    virtual QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query = QNetworkProxyQuery()) override;

private:
    QMutex m_mutex;
    QMap<QString, QNetworkProxy> m_proxyInfo;

    QNetworkProxy getProxyToResource( const QnResourcePtr& resource );
};

#endif
