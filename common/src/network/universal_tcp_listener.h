#ifndef __UNIVERSAL_TCP_LISTENER_H__
#define __UNIVERSAL_TCP_LISTENER_H__

#include <QMultiMap>
#include <QWaitCondition>
#include <QtCore/QElapsedTimer>

#include "utils/network/tcp_listener.h"
#include "utils/network/tcp_connection_processor.h"
#include "utils/network/http/httptypes.h"


template <class T>
QnTCPConnectionProcessor* handlerInstance(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner)
{
    return new T(socket, owner);
};

class QnUniversalTcpListener: public QnTcpListener
{
private:
public:
    typedef QnTCPConnectionProcessor* (*InstanceFunc)(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner);
    struct HandlerInfo
    {
        QByteArray protocol;
        QString path;
        InstanceFunc instanceFunc;
    };

    static const int DEFAULT_RTSP_PORT = 554;

    explicit QnUniversalTcpListener(
        const QHostAddress& address = QHostAddress::Any,
        int port = DEFAULT_RTSP_PORT,
        int maxConnections = QnTcpListener::DEFAULT_MAX_CONNECTIONS,
        bool useSsl = false );
    virtual ~QnUniversalTcpListener();
    
    template <class T> 
    void addHandler(const QByteArray& protocol, const QString& path)
    {
        HandlerInfo handler;
        handler.protocol = protocol;
        handler.path = path;
        handler.instanceFunc = handlerInstance<T>;
        m_handlers.append(handler);
    }

    typedef std::function<bool(const nx_http::Request&)> ProxyCond;
    struct ProxyInfo
    {
        ProxyInfo(): proxyHandler(0) {}
        InstanceFunc proxyHandler;
        ProxyCond proxyCond;
    };

    template <class T>
    void setProxyHandler( const ProxyCond& cond )
    {
        m_proxyInfo.proxyHandler = handlerInstance<T>;
        m_proxyInfo.proxyCond = cond;
    }

    InstanceFunc findHandler(QSharedPointer<AbstractStreamSocket> clientSocket, const QByteArray& protocol, const nx_http::Request& request);
    QnTCPConnectionProcessor* createNativeProcessor(QSharedPointer<AbstractStreamSocket> clientSocket, const QByteArray& protocol, const nx_http::Request& request);

    /* proxy support functions */

    void setProxyParams(const QUrl& proxyServerUrl, const QString& selfId);
    void addProxySenderConnections(int size);

    bool registerProxyReceiverConnection(const QString& url, QSharedPointer<AbstractStreamSocket> socket);
    QSharedPointer<AbstractStreamSocket> getProxySocket(const QString& guid, int timeout);
    void setProxyPoolSize(int value);

    void disableAuth();

    bool isProxy(const nx_http::Request& request);
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket, QnTcpListener* owner) override;
    virtual void doPeriodicTasks() override;
private:
    struct AwaitProxyInfo
    {
        explicit AwaitProxyInfo(QSharedPointer<AbstractStreamSocket> _socket): socket(_socket) { timer.restart(); }
        AwaitProxyInfo() { timer.restart(); }

        QSharedPointer<AbstractStreamSocket> socket;
        QElapsedTimer timer;
    };

    QList<HandlerInfo> m_handlers;
    ProxyInfo m_proxyInfo;
    QUrl m_proxyServerUrl;
    QString m_selfIdForProxy;
    QMutex m_proxyMutex;
    QWaitCondition m_proxyWaitCond;

    typedef QMultiMap<QString, AwaitProxyInfo> ProxyList;
    ProxyList m_awaitingProxyConnections;

    QSet<QString> m_proxyConExists;
    int m_proxyPoolSize;
    bool m_needAuth;
};

#endif // __UNIVERSAL_TCP_LISTENER_H__
