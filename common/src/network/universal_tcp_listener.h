#ifndef __UNIVERSAL_TCP_LISTENER_H__
#define __UNIVERSAL_TCP_LISTENER_H__

#include <QMultiMap>
#include <QWaitCondition>
#include <QtCore/QElapsedTimer>

#include "utils/network/tcp_listener.h"
#include "utils/network/tcp_connection_processor.h"
#include "utils/network/http/httptypes.h"


class QnUniversalTcpListener;

template <class T>
QnTCPConnectionProcessor* handlerInstance(QSharedPointer<AbstractStreamSocket> socket, QnUniversalTcpListener* owner)
{
    return new T(socket, owner);
};

class QnUniversalTcpListener: public QnTcpListener
{
private:
public:
    typedef QnTCPConnectionProcessor* (*InstanceFunc)(QSharedPointer<AbstractStreamSocket> socket, QnUniversalTcpListener* owner);
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

    InstanceFunc findHandler(const QByteArray& protocol, const nx_http::Request& request);

    /* proxy support functions */

    void addProxySenderConnections(const SocketAddress& proxyUrl, int size);

    bool registerProxyReceiverConnection(const QString& url, QSharedPointer<AbstractStreamSocket> socket);

    typedef std::function<void(int count)> SocketRequest;
    QSharedPointer<AbstractStreamSocket> getProxySocket(
            const QString& guid, int timeout, const SocketRequest& socketRequest = SocketRequest());

    void disableAuth();

    bool isProxy(const nx_http::Request& request);
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket) override;
    virtual void doPeriodicTasks() override;

private:
    struct AwaitProxyInfo
    {
        explicit AwaitProxyInfo(const QSharedPointer<AbstractStreamSocket>& _socket)
            : socket(_socket) { timer.restart(); }

        QSharedPointer<AbstractStreamSocket> socket;
        QElapsedTimer timer;
    };

    QList<HandlerInfo> m_handlers;
    ProxyInfo m_proxyInfo;
    QMutex m_proxyMutex;
    QMap<QString, QList<AwaitProxyInfo>> m_proxyPool;
    QWaitCondition m_proxyCondition;
    bool m_needAuth;
};

#endif // __UNIVERSAL_TCP_LISTENER_H__
