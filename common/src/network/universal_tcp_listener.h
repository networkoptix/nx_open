#ifndef __UNIVERSAL_TCP_LISTENER_H__
#define __UNIVERSAL_TCP_LISTENER_H__

#include <QMultiMap>
#include <QWaitCondition>
#include <QtCore/QTime>
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
    struct HandlerInfo
    {
        QByteArray protocol;
        QString path;
        QnTCPConnectionProcessor* (*instanceFunc)(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner);
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

    QnTCPConnectionProcessor* createNativeProcessor(QSharedPointer<AbstractStreamSocket> clientSocket, const QByteArray& protocol, const QString& path);

    /* proxy support functions */

    void setProxyParams(const QUrl& proxyServerUrl, const QString& selfId);
    void addProxySenderConnections(int size);

    bool registerProxyReceiverConnection(const QString& url, QSharedPointer<AbstractStreamSocket> socket);
    QSharedPointer<AbstractStreamSocket> getProxySocket(const QString& guid, int timeout);
    void setProxyPoolSize(int value);

    void disableAuth();
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket, QnTcpListener* owner) override;
    virtual void doPeriodicTasks() override;
private:
    struct AwaitProxyInfo
    {
        explicit AwaitProxyInfo(QSharedPointer<AbstractStreamSocket> _socket): socket(_socket) { timer.restart(); }
        AwaitProxyInfo() { timer.restart(); }

        QSharedPointer<AbstractStreamSocket> socket;
        QTime timer;
    };

    QList<HandlerInfo> m_handlers;
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
