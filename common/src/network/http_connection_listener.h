#pragma once

#include <boost/optional.hpp>
#include <QMultiMap>
#include <QtCore/QElapsedTimer>

#include <nx/utils/thread/wait_condition.h>

#include <network/tcp_listener.h>
#include <network/tcp_connection_processor.h>

#include <nx/network/http/http_types.h>
#include <rest/server/rest_connection_processor.h>

class QnHttpConnectionListener;

template<class T>
QnTCPConnectionProcessor* handlerInstance(
    QSharedPointer<nx::network::AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
{
    return new T(socket, owner);
};

template <class T, class ExtraParam>
QnTCPConnectionProcessor* handlerInstance(
    ExtraParam extraParam,
    QSharedPointer<nx::network::AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
{
    return new T(extraParam, socket, owner);
}

class QnHttpConnectionListener: public QnTcpListener
{
public:
    typedef std::function<QnTCPConnectionProcessor*(
        QSharedPointer<nx::network::AbstractStreamSocket>, QnHttpConnectionListener*)> InstanceFunc;

    struct HandlerInfo
    {
        QByteArray protocol;
        QString path;
        InstanceFunc instanceFunc;
    };

    static const int kDefaultRtspPort = 554;

    explicit QnHttpConnectionListener(
        QnCommonModule* commonModule,
        const QHostAddress& address = QHostAddress::Any,
        int port = kDefaultRtspPort,
        int maxConnections = DEFAULT_MAX_CONNECTIONS,
        bool useSsl = false);

    virtual ~QnHttpConnectionListener();

    template<class T, class ExtraParam>
    void addHandler(const QByteArray& protocol, const QString& path, ExtraParam extraParam)
    {
        using namespace std::placeholders;
        doAddHandler(protocol, path,
            std::bind(&handlerInstance<T, ExtraParam>, extraParam, _1, _2));
    }

    template<class T>
    void addHandler(const QByteArray& protocol, const QString& path)
    {
        // NOTE: Cast to InstanceFunc is a workaround for MSVC deficiency.
        doAddHandler(protocol, path, (InstanceFunc) handlerInstance<T>);
    }

    typedef std::function<bool(QnCommonModule* commonModule, const nx::network::http::Request&)> ProxyCond;

    struct ProxyInfo
    {
        ProxyInfo(): proxyHandler(0) {}
        InstanceFunc proxyHandler;
        ProxyCond proxyCond;
    };

    template <class T>
    void setProxyHandler(const ProxyCond& cond)
    {
        m_proxyInfo.proxyHandler = handlerInstance<T>;
        m_proxyInfo.proxyCond = cond;
    }

    template <class T, class ExtraParam>
    void setProxyHandler(const ProxyCond& cond, ExtraParam extraParam)
    {
        using namespace std::placeholders;
        m_proxyInfo.proxyHandler = std::bind(&handlerInstance<T, ExtraParam>, extraParam, _1, _2);
        m_proxyInfo.proxyCond = cond;
    }

    InstanceFunc findHandler(const QByteArray& protocol, const nx::network::http::Request& request);

    // Proxy support functions.

    bool registerProxyReceiverConnection(
        const QString& url, QSharedPointer<nx::network::AbstractStreamSocket> socket);

    typedef std::function<void(int count)> SocketRequest;

    QSharedPointer<nx::network::AbstractStreamSocket> getProxySocket(
        const QString& guid, int timeout, const SocketRequest& socketRequest);

    void disableAuth();

    bool isProxy(const nx::network::http::Request& request);
    bool needAuth() const;

    QnRestProcessorPool* processorPool() { return &m_processorPool; }

protected:
    virtual void doPeriodicTasks() override;

private:
    void doAddHandler(const QByteArray& protocol, const QString& path, InstanceFunc instanceFunc);

    struct AwaitProxyInfo
    {
        explicit AwaitProxyInfo(const QSharedPointer<nx::network::AbstractStreamSocket>& socket):
            socket(socket)
        {
            timer.restart();
        }

        QSharedPointer<nx::network::AbstractStreamSocket> socket;
        QElapsedTimer timer;
    };

    struct ServerProxyPool
    {
        ServerProxyPool(): requested(0) {}

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
