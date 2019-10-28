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

namespace nx::vms::network {

class ReverseConnectionManager:
    public QObject,
    public AbstractServerConnector
{
public:
    ReverseConnectionManager(QnHttpConnectionListener* tcpListener);
    virtual ~ReverseConnectionManager() override;

    void startReceivingNotifications(ec2::AbstractECConnection* connection);
    bool saveIncomingConnection(const QnUuid& peerId, Connection connection);

    cf::future<Connection> connect(
        const QnRoute& route,
        std::chrono::milliseconds timeout,
        bool sslRequired = true) override;

private:
    struct IncomingConnections
    {
        std::list<Connection> connections;
        nx::utils::ElapsedTimer requestTimer;
        size_t requested = 0;
        std::multimap<std::chrono::steady_clock::time_point, cf::promise<Connection>> promises;
        nx::network::aio::Timer promiseTimer;
    };

private:
    void onReverseConnectionRequest(const nx::vms::api::ReverseConnectionData& data);
    void onOutgoingConnectDone(nx::network::http::AsyncClient* httpClient);

    cf::future<Connection> reverseConnectTo(const QnUuid& id, std::chrono::milliseconds timeout);
    void restartPromiseTimer(const QnUuid& id, IncomingConnections* peer, std::chrono::milliseconds timeout);
    void requestReverseConnections(const QnUuid& id, IncomingConnections* peer);

private:
    QnHttpConnectionListener* const m_tcpListener = nullptr;
    mutable QnMutex m_mutex;
    std::set<std::unique_ptr<nx::network::http::AsyncClient>> m_outgoingClients;
    std::map<QnUuid, IncomingConnections> m_incomingConnections;
};

} // namespace nx::vms::network
