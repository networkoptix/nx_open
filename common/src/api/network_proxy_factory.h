#ifndef QN_NETWORK_PROXY_FACTORY_H
#define QN_NETWORK_PROXY_FACTORY_H

#include <QtNetwork/qnetworkproxy.h>
#include <QtCore/QPointer>

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

    void removeFromProxyList(const QUrl& url);

    void addToProxyList(const QUrl& url, const QString& addr, int port);

    void clearProxyList();

    static QnNetworkProxyFactory* instance();

protected:
    virtual QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query = QNetworkProxyQuery()) override;

private:
    struct ProxyInfo {
        ProxyInfo(): port(0) {}
        ProxyInfo(const QString& _addr, int _port): addr(_addr), port(_port) {}
        QString addr;
        int port;
    };
    QMutex m_mutex;
    QMap<QString, ProxyInfo> m_proxyInfo;
};

#endif
