
#include "network_proxy_factory.h"

#include <api/app_server_connection.h>
#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>
#include <network/authenticate_helper.h>


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
    QMutexLocker lk( &m_mutex );
    m_proxyInfo[targetHost] = QNetworkProxy(QNetworkProxy::HttpProxy, proxyHost, proxyPort, userName, password);
}

void QnNetworkProxyFactory::bindHostToResource(
    const QString& targetHost,
    const QnResourcePtr& resource )
{
    QMutexLocker lk( &m_mutex );
    m_proxyInfo[targetHost] = getProxyToResource( resource );
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

bool QnNetworkProxyFactory::fillUrlWithRouteToResource(
    const QnResourcePtr& targetResource,
    QUrl* const requestUrl,
    WhereToPlaceProxyCredentials credentialsBehavour )
{
    QUrlQuery urlQuery( requestUrl->query() );
    const QNetworkProxy& proxy = getProxyToResource( targetResource );
    switch( proxy.type() )
    {
        case QNetworkProxy::NoProxy:
            break;

        case QNetworkProxy::HttpProxy:
            requestUrl->setPath( lit("/proxy/%1%2").arg(targetResource->getId().toString()).arg(requestUrl->path()) );
            requestUrl->setHost( proxy.hostName() );
            requestUrl->setPort( proxy.port() );
            urlQuery.addQueryItem( lit("proxy_auth"), QLatin1String(QnAuthHelper::createHttpQueryAuthParam( requestUrl->userName(), requestUrl->password() )) );
            break;

        default:
            assert( false );
            return false;
    }

    if( !requestUrl->userName().isEmpty() )
    {
        //adding credentials
        switch( credentialsBehavour )
        {
            case QnNetworkProxyFactory::placeCredentialsToUrl:
                urlQuery.addQueryItem( lit("auth"), QLatin1String(QnAuthHelper::createHttpQueryAuthParam( requestUrl->userName(), requestUrl->password() )) );
                break;

            default:
                //TODO #ak
                break;
        }
    }

    requestUrl->setQuery( urlQuery );
    return true;
}

QnNetworkProxyFactory* QnNetworkProxyFactory::instance()
{
    return QnNetworkProxyFactory_instance;
}

QNetworkProxy QnNetworkProxyFactory::getProxyToResource( const QnResourcePtr& resource )
{
    QNetworkProxy proxy( QNetworkProxy::HttpProxy );
    if( dynamic_cast<QnSecurityCamResource*>(resource.data()) )
    {
        QnResourcePtr parent = resource->getParentResource();
        if( !parent )
            return QNetworkProxy( QNetworkProxy::NoProxy );
        const QnMediaServerResource* mediaServerResource = dynamic_cast<QnMediaServerResource*>(parent.data());
        if( !mediaServerResource )
            return QNetworkProxy( QNetworkProxy::NoProxy );
        const QString& proxyHost = mediaServerResource->getPrimaryIF();
        if( proxyHost == QnMediaServerResource::USE_PROXY )
        {
            //proxying via Server
            proxy.setHostName( QnAppServerConnectionFactory::url().host() );
            proxy.setPort( QnAppServerConnectionFactory::url().port() );
        }
        else
        {
            //going directly through mediaserver
            const QUrl mServerApiUrl( mediaServerResource->getApiUrl() );
            proxy.setHostName( mServerApiUrl.host() );
            proxy.setPort( mServerApiUrl.port() );
        }
        proxy.setUser( QnAppServerConnectionFactory::url().userName() );
        proxy.setPassword( QnAppServerConnectionFactory::url().password() );
        return proxy;
    }
    else if( const QnMediaServerResource* mediaServerResource = dynamic_cast<QnMediaServerResource*>(resource.data()) )
    {
        //TODO #ak looks like copy-paste from upper block
        const QString& proxyHost = mediaServerResource->getPrimaryIF();
        if( proxyHost == QnMediaServerResource::USE_PROXY )
        {
            const QUrl proxyUrl = QnAppServerConnectionFactory::url();
            //proxying via Server
            proxy.setHostName( proxyUrl.host() );
            proxy.setPort( proxyUrl.port() );
            proxy.setUser( proxyUrl.userName() );
            proxy.setPassword( proxyUrl.password() );
            return proxy;
        }
        else
        {
            //going directly through mediaserver
            return QNetworkProxy( QNetworkProxy::NoProxy );
        }
    }
    else
    {
        return QNetworkProxy( QNetworkProxy::NoProxy );
    }
}
