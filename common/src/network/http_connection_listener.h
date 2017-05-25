#ifndef __HTTP_CONNECTION_LISTENER_H__
#define __HTTP_CONNECTION_LISTENER_H__

#include <QMultiMap>
#include <nx/utils/thread/wait_condition.h>
#include <QtCore/QElapsedTimer>

#include "network/tcp_listener.h"
#include "network/tcp_connection_processor.h"

#include <nx/network/http/http_types.h>
#include <rest/server/rest_connection_processor.h>

class QnHttpConnectionListener;

template <class T>
QnTCPConnectionProcessor* handlerInstance(
    QSharedPointer<AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
{
    return new T(socket, owner);
};

template <class T, class ExtraParam>
QnTCPConnectionProcessor* handlerInstance(
    ExtraParam* extraParam,
    QSharedPointer<AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
{
    return new T(extraParam, socket, owner);
};

class QnHttpConnectionListener: public QnTcpListener
{
private:
public:
    typedef std::function<QnTCPConnectionProcessor*(QSharedPointer<AbstractStreamSocket>, QnHttpConnectionListener*)> InstanceFunc;

    struct HandlerInfo
    {
        QByteArray protocol;
        QString path;
        InstanceFunc instanceFunc;
    };

    static const int DEFAULT_RTSP_PORT = 554;

    explicit QnHttpConnectionListener(
        QnCommonModule* commonModule,
        const QHostAddress& address = QHostAddress::Any,
        int port = DEFAULT_RTSP_PORT,
        int maxConnections = QnTcpListener::DEFAULT_MAX_CONNECTIONS,
        bool useSsl = false );
    virtual ~QnHttpConnectionListener();

    template <class T>
    void addHandler(const QByteArray& protocol, const QString& path)
    {
        HandlerInfo handler;
        handler.protocol = protocol;
        handler.path = path;
        handler.instanceFunc = handlerInstance<T>;
        m_handlers.append(handler);
    }

    template <class T, class ExtraParam>
    void addHandler(const QByteArray& protocol, const QString& path, ExtraParam* extraParam)
    {
        using namespace std::placeholders;

        HandlerInfo handler;
        handler.protocol = protocol;
        handler.path = path;
        handler.instanceFunc = std::bind(&handlerInstance<T, ExtraParam>, extraParam, _1, _2);
        m_handlers.append(handler);
    }

    typedef std::function<bool(QnCommonModule* commonModule, const nx_http::Request&)> ProxyCond;
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

    template <class T, class ExtraParam>
    void setProxyHandler(const ProxyCond& cond, ExtraParam* extraParam)
    {
        using namespace std::placeholders;

        m_proxyInfo.proxyHandler = std::bind(&handlerInstance<T, ExtraParam>, extraParam, _1, _2);
        m_proxyInfo.proxyCond = cond;
    }

    InstanceFunc findHandler(const QByteArray& protocol, const nx_http::Request& request);

    /* proxy support functions */

    bool registerProxyReceiverConnection(const QString& url, QSharedPointer<AbstractStreamSocket> socket);

    typedef std::function<void(int count)> SocketRequest;
    QSharedPointer<AbstractStreamSocket> getProxySocket(
            const QString& guid, int timeout, const SocketRequest& socketRequest);

    void disableAuth();

    bool isProxy(const nx_http::Request& request);
    bool needAuth() const;

    QnRestProcessorPool* processorPool() { return &m_processorPool; }
protected:
    virtual void doPeriodicTasks() override;

private:
    struct AwaitProxyInfo
    {
        explicit AwaitProxyInfo(const QSharedPointer<AbstractStreamSocket>& _socket)
            : socket(_socket) { timer.restart(); }

        QSharedPointer<AbstractStreamSocket> socket;
        QElapsedTimer timer;
    };

    struct ServerProxyPool
    {
        ServerProxyPool() : requested(0) {}

        size_t requested;
        QList<AwaitProxyInfo> available;
        QElapsedTimer timer;
    };

    QList<HandlerInfo> m_handlers;
    ProxyInfo m_proxyInfo;
    QnMutex m_proxyMutex;
    QMap<QString, ServerProxyPool> m_proxyPool;
    QnWaitCondition m_proxyCondition;
    bool m_needAuth;
    QnRestProcessorPool m_processorPool;
};

#endif // __HTTP_CONNECTION_LISTENER_H__
