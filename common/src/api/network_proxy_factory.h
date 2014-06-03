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

    void addToProxyList(
        const QUrl& targetUrl,
        const QString& proxyHost,
        quint16 proxyPort,
        const QString& userName = QString(),
        const QString& password = QString() );

    void clearProxyList();

    static QnNetworkProxyFactory* instance();

protected:
    virtual QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query = QNetworkProxyQuery()) override;

private:
    QMutex m_mutex;
    QMap<QString, QNetworkProxy> m_proxyInfo;
};

#endif
