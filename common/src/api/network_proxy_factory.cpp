#include "network_proxy_factory.h"

// -------------------------------------------------------------------------- //
// QnNetworkProxyFactory
// -------------------------------------------------------------------------- //
/**
 * Note that instance of this class will be used from several threads, and
 * must therefore be thread-safe.
 */
QnNetworkProxyFactory::QnNetworkProxyFactory()
{
}

QnNetworkProxyFactory::~QnNetworkProxyFactory()
{
}

void QnNetworkProxyFactory::removeFromProxyList(const QUrl& url)
{
    QMutexLocker locker(&m_mutex);
    QString host = url.host();//QString(QLatin1String("%1:%2")).arg(url.host()).arg(url.port());

    m_proxyInfo.remove(host);
}

void QnNetworkProxyFactory::addToProxyList(const QUrl& url, const QString& addr, int port)
{
    QMutexLocker locker(&m_mutex);
    QString host = url.host();//QString(QLatin1String("%1:%2")).arg(url.host()).arg(url.port());

    m_proxyInfo.insert(host, ProxyInfo(addr, port));
}

void QnNetworkProxyFactory::clearProxyList()
{
    QMutexLocker locker(&m_mutex);

    m_proxyInfo.clear();
}

QList<QNetworkProxy> QnNetworkProxyFactory::queryProxy(const QNetworkProxyQuery &query)
{
    QList<QNetworkProxy> rez;
    QString host_name = query.url().host();

    QString urlPath = query.url().path();    
    
    if (urlPath.startsWith(QLatin1String("/")))
        urlPath.remove(0, 1);

    if (urlPath.endsWith(QLatin1String("/")))
        urlPath.chop(1);

    
    if (/*urlPath.isEmpty() || */urlPath == QLatin1String("api/ping")) {
        rez << QNetworkProxy(QNetworkProxy::NoProxy);
        return rez;
    }
    QUrl url = query.url();
    

    QString url_host = url.host();
    int port = url.port();


    QString userAdmin = url.userName();
    QString userPassword = url.password();


    url.setPath(QString());
    url.setUserInfo(QString());
    url.setQuery(QUrlQuery());


    QMutexLocker locker(&m_mutex);

    QMap<QString, ProxyInfo>::const_iterator itr;
    for ( itr = m_proxyInfo.begin() ; itr != m_proxyInfo.end() ; itr++ )
    {
        qDebug() << "proxy can be" << itr.key();
    }

    QString host = url_host;//QString(QLatin1String("%1:%2")).arg(url_host).arg(port);

    itr = m_proxyInfo.find(host);
    if (itr == m_proxyInfo.end())
    {
        qDebug() << "proxy disagree" << urlPath << "   " << url;
        rez << QNetworkProxy(QNetworkProxy::NoProxy);
    }
    else
    {
        qDebug() << "proxy agree" << urlPath << "   " << url << "proxy url" << itr.value().addr << "proxy port"<<  itr.value().port;
        QNetworkProxy proxy(QNetworkProxy::HttpProxy, itr.value().addr, itr.value().port,userAdmin,userPassword);
        rez << proxy;
    }
    return rez;
}

QPointer<QnNetworkProxyFactory> createGlobalProxyFactory() {
    QnNetworkProxyFactory *result(new QnNetworkProxyFactory());

    /* Qt will take ownership of the supplied instance. */
    QNetworkProxyFactory::setApplicationProxyFactory(result); // TODO: #Elric we have a race if this code is run several times from different threads.

    return result;
}

QnNetworkProxyFactory *QnNetworkProxyFactory::instance()
{
    QPointer<QnNetworkProxyFactory> *result = qn_globalProxyFactory();
    if(*result) {
        return result->data();
    } else {
        return qn_reserveProxyFactory();
    }
}