#pragma once

#include <deque>
#include <set>
#include <deque>

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <nx/network/abstract_socket.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/elapsed_timer.h>
#include <network/http_connection_listener.h>
#include <nx/vms/network/abstract_server_connector.h>
#include <nx/network/http/http_async_client.h>
#include <nx/vms/api/data/reverse_connection_data.h>
#include <nx_ec/ec_api.h>

namespace nx {
namespace vms {
namespace network {

class ReverseConnectionManager:
    public QObject,
    public QnCommonModuleAware,
    public AbstractServerConnector
{
    Q_OBJECT
public:
    ReverseConnectionManager(QnHttpConnectionListener* tcpListener);
    virtual ~ReverseConnectionManager() override;

    void startReceivingNotifications(ec2::AbstractECConnection* connection);

    bool addIncomingTcpConnection(
        const QString& url,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket);

    /**
     * @return Tcp socket connected to specified media server.
     * @param guid Media server guid.
     * @param timeoutMs total timeout to establish reverse connection.
     */
    std::unique_ptr<nx::network::AbstractStreamSocket> getProxySocket(
        const QnUuid& guid, std::chrono::milliseconds timeout);

    virtual std::unique_ptr<nx::network::AbstractStreamSocket> connectTo(
        const QnRoute& route, bool sslRequired,std::chrono::milliseconds timeout) override;

private slots:
    void at_reverseConnectionRequested(const nx::vms::api::ReverseConnectionData& data);

private:
    void onHttpClientDone(nx::network::http::AsyncClient* httpClient);

    void at_socketReadTimeout(
        const QnUuid& serverId,
        nx::network::AbstractStreamSocket* socket,
        SystemError::ErrorCode errorCode,
        size_t bytesRead);
private:
    mutable QnMutex m_mutex;

    std::set<std::unique_ptr<nx::network::http::AsyncClient>> m_runningHttpClients;

    struct SocketData
    {
        SocketData(): tmpReadBuffer(new QByteArray())
        {
            tmpReadBuffer->reserve(32);
        }

        SocketData(SocketData&& value) = default;
        SocketData& operator=(SocketData&& value) = default;

        ~SocketData()
        {
            if (socket)
                socket->pleaseStopSync();
        }

        std::unique_ptr<nx::network::AbstractStreamSocket> socket;
        std::unique_ptr<QByteArray> tmpReadBuffer;
    };

    struct PreparedSocketPool
    {
        std::deque<SocketData> sockets;
        nx::utils::ElapsedTimer timer;
        int requested = 0;
    };

    std::map<QnUuid, PreparedSocketPool> m_preparedSockets;
    QnWaitCondition m_proxyCondition;

    QnHttpConnectionListener* m_tcpListener = nullptr;
private:
    SocketData getPreparedSocketUnsafe(const QnUuid& guid);
};

} // namespace network
} // namespace vms
} // namespace nx
