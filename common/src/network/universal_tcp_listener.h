#ifndef __UNIVERSAL_TCP_LISTENER_H__
#define __UNIVERSAL_TCP_LISTENER_H__

#include <QMultiMap>
#include <QWaitCondition>
#include "utils/network/tcp_listener.h"
#include "utils/network/tcp_connection_processor.h"

template <class T>
QnTCPConnectionProcessor* handlerInstance(TCPSocket* socket, QnTcpListener* owner)
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
        QnTCPConnectionProcessor* (*instanceFunc)(TCPSocket* socket, QnTcpListener* owner);
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

    QnTCPConnectionProcessor* createNativeProcessor(TCPSocket* clientSocket, const QByteArray& protocol, const QString& path);

    /* proxy support functions */

    void setProxyParams(const QUrl& proxyServerUrl, const QString& selfId);
    void addProxySenderConnections(int size);

    bool registerProxyReceiverConnection(const QString& url, TCPSocket* socket);
    TCPSocket* getProxySocket(const QString& guid, int timeout);
    void setProxyPoolSize(int value);
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner);
private:
    QList<HandlerInfo> m_handlers;
    QUrl m_proxyServerUrl;
    QString m_selfIdForProxy;
    QMutex m_proxyMutex;
    QWaitCondition m_proxyWaitCond;
    QMap<QString, TCPSocket*> m_awaitingProxyConnections;
    QSet<QString> m_proxyConExists;
    int m_proxyPoolSize;
};

#endif // __UNIVERSAL_TCP_LISTENER_H__
