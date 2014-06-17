#ifndef __UNIVERSAL_TCP_LISTENER_H__
#define __UNIVERSAL_TCP_LISTENER_H__

#include <QMultiMap>
#include <QWaitCondition>
#include <QtCore/QElapsedTimer>

#include "utils/network/tcp_listener.h"
#include "utils/network/tcp_connection_processor.h"

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

    explicit QnUniversalTcpListener(const QHostAddress& address = QHostAddress::Any, int port = DEFAULT_RTSP_PORT, int maxConnections = 1000);
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

    typedef bool (*ProxyCond)(void* opaque, const QUrl& url);
    struct ProxyInfo
    {
        ProxyInfo(): proxyHandler(0), proxyCond(0), proxyOpaque(0) {}
        InstanceFunc proxyHandler;
        ProxyCond proxyCond;
        void *proxyOpaque;
    };

    template <class T>
    void setProxyHandler(void* opaque, ProxyCond cond)
    {
        m_proxyInfo.proxyHandler = handlerInstance<T>;
        m_proxyInfo.proxyCond = cond;
        m_proxyInfo.proxyOpaque = opaque;
    }

    QnTCPConnectionProcessor* createNativeProcessor(QSharedPointer<AbstractStreamSocket> clientSocket, const QByteArray& protocol, const QUrl& url);

    /* proxy support functions */

    void setProxyParams(const QUrl& proxyServerUrl, const QString& selfId);
    void addProxySenderConnections(int size);

    bool registerProxyReceiverConnection(const QString& url, QSharedPointer<AbstractStreamSocket> socket);
    QSharedPointer<AbstractStreamSocket> getProxySocket(const QString& guid, int timeout);
    void setProxyPoolSize(int value);

    void disableAuth();

    bool isProxy(const QUrl& url);
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
