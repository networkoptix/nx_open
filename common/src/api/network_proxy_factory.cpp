#include "network_proxy_factory.h"

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

void QnNetworkProxyFactory::removeFromProxyList(const QUrl& url)
{
    QMutexLocker locker(&m_mutex);

    m_proxyInfo.remove(url.host());
}

void QnNetworkProxyFactory::addToProxyList(
    const QUrl& targetUrl,
    const QString& proxyHost,
    quint16 proxyPort,
    const QString& userName,
    const QString& password )
{
    QMutexLocker locker(&m_mutex);

    m_proxyInfo.insert(
        targetUrl.host(),
        QNetworkProxy(QNetworkProxy::HttpProxy, proxyHost, proxyPort, userName, password) );
}

void QnNetworkProxyFactory::clearProxyList()
{
    QMutexLocker locker(&m_mutex);

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

    QMap<QString, QNetworkProxy>::const_iterator itr = m_proxyInfo.find(query.url().host());
    if( itr == m_proxyInfo.end() )
        return QList<QNetworkProxy>()<<QNetworkProxy(QNetworkProxy::NoProxy);
    return QList<QNetworkProxy>() << itr.value();
}

QnNetworkProxyFactory *QnNetworkProxyFactory::instance()
{
    return QnNetworkProxyFactory_instance;
}
