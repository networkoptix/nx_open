
#include "network_proxy_factory.h"

#include <api/app_server_connection.h>
#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>


// -------------------------------------------------------------------------- //
// QnNetworkProxyFactory
// -------------------------------------------------------------------------- //
static QnNetworkProxyFactory* QnNetworkProxyFactory_instance = nullptr;

QnNetworkProxyFactory::QnNetworkProxyFactory()
{
    Q_ASSERT( QnNetworkProxyFactory_instance == nullptr );
    QnNetworkProxyFactory_instance = this;
}

QnNetworkProxyFactory::~QnNetworkProxyFactory()
{
    QnNetworkProxyFactory_instance = nullptr;
}

void QnNetworkProxyFactory::removeFromProxyList(const QString& targetHost)
{
    QMutexLocker locker(&m_mutex);

    m_proxyInfo.remove(targetHost);
}

void QnNetworkProxyFactory::addToProxyList(
    const QString& targetHost,
    const QString& proxyHost,
    quint16 proxyPort,
    const QString& userName,
    const QString& password )
{
    QMutexLocker l( &m_mutex );

    m_proxyInfo[targetHost] = QNetworkProxy(QNetworkProxy::HttpProxy, proxyHost, proxyPort, userName, password);
}

void QnNetworkProxyFactory::bindHostToResource(
    const QString& targetHost,
    QnResourcePtr resource )
{
    QMutexLocker lk( &m_mutex );

    QNetworkProxy proxy( QNetworkProxy::HttpProxy );
    QnSecurityCamResourcePtr camResource = resource.dynamicCast<QnSecurityCamResource>();
    if( camResource )
    {
        QnResourcePtr parent = resource->getParentResource();
        if( !parent )
            return; //no proxy
        QnMediaServerResourcePtr mediaServerResource = parent.dynamicCast<QnMediaServerResource>();
        if( !mediaServerResource )
            return; //no proxy
        const QString& proxyHost = mediaServerResource->getPrimaryIF();
        if( proxyHost == QnMediaServerResource::USE_PROXY )
        {
            //proxying via EC
            proxy.setHostName( QnAppServerConnectionFactory::defaultUrl().host() );
            proxy.setPort( QnAppServerConnectionFactory::defaultUrl().port() );
        }
        else
        {
            //going directly through mediaserver
            const QUrl mServerApiUrl( mediaServerResource->getApiUrl() );
            proxy.setHostName( mServerApiUrl.host() );
            proxy.setPort( mServerApiUrl.port() );
        }
    }
    
    proxy.setUser( QnAppServerConnectionFactory::defaultUrl().userName() );
    proxy.setPassword( QnAppServerConnectionFactory::defaultUrl().password() );
    m_proxyInfo[targetHost] = proxy;
    return;
}

void QnNetworkProxyFactory::clearProxyList()
{
    QMutexLocker lk( &m_mutex );

    m_proxyInfo.clear();
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

    QMutexLocker locker(&m_mutex);

    const QString& targetHost = query.url().host();
    QMap<QString, QNetworkProxy>::const_iterator itr = m_proxyInfo.find(targetHost);
    if( itr == m_proxyInfo.end() )
        return QList<QNetworkProxy>() << QNetworkProxy(QNetworkProxy::NoProxy);
    return QList<QNetworkProxy>() << itr.value();
}

QnNetworkProxyFactory *QnNetworkProxyFactory::instance()
{
    return QnNetworkProxyFactory_instance;
}
