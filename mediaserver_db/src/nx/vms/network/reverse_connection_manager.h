#pragma once

#include <deque>

#include <QtCore/QSharedPointer>

#include <nx/network/abstract_socket.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/elapsed_timer.h>
#include <network/http_connection_listener.h>
#include <nx/vms/network/abstract_server_connector.h>

namespace nx {
namespace vms {
namespace network {

class ReverseConnectionManager: 
    public QnCommonModuleAware, 
    public AbstractServerConnector
{
    Q_OBJECT
public:
    ReverseConnectionManager(QnHttpConnectionListener* tcpListener);
    virtual ~ReverseConnectionManager() override;

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

    virtual std::unique_ptr<nx::network::AbstractStreamSocket> connect(
        const QnRoute& route, std::chrono::milliseconds timeout) override;
    private slots:
    void at_reverseConnectionRequested(const ec2::ApiReverseConnectionData& data);
private:
    void doPeriodicTasks();
    void onHttpClientDone(nx::network::http::AsyncClient* httpClient);
private:
    mutable QnMutex m_mutex;
    std::set<std::unique_ptr<nx::network::http::AsyncClient>> m_runningHttpClients;

    using PreparedSocketPool = std::vector<std::unique_ptr<nx::network::AbstractStreamSocket>>;
    std::map<QnUuid, PreparedSocketPool> m_preparedSockets;
    QnWaitCondition m_proxyCondition;
    QnHttpConnectionListener* m_tcpListener = nullptr;
};

} // namespace network
} // namespace vms
} // namespace nx
