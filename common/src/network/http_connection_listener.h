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
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
{
    return new T(std::move(socket), owner);
};

template <class T, class ExtraParam>
QnTCPConnectionProcessor* handlerInstance(
    ExtraParam extraParam,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
{
    return new T(extraParam, std::move(socket), owner);
}

class QnHttpConnectionListener: public QnTcpListener
{
public:
    typedef std::function<QnTCPConnectionProcessor*(
        std::unique_ptr<nx::network::AbstractStreamSocket>, QnHttpConnectionListener*)> InstanceFunc;

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

    void disableAuth();

    bool isProxy(const nx::network::http::Request& request);
    bool needAuth() const;

    QnRestProcessorPool* processorPool() { return &m_processorPool; }

private:
    void doAddHandler(const QByteArray& protocol, const QString& path, InstanceFunc instanceFunc);



    QList<HandlerInfo> m_handlers;
    ProxyInfo m_proxyInfo;
    bool m_needAuth;
    QnRestProcessorPool m_processorPool;
};
