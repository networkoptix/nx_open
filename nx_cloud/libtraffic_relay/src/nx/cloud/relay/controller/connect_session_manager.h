#pragma once

#include <memory>
#include <list>

#include <nx/network/cloud/tunnel/relay/api/relay_api_data_types.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_result_code.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/utils/counter.h>
#include <nx/utils/thread/cf/async_queued_executor.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

#include <nx/cloud/relaying/listening_peer_pool.h>

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; };

namespace model {
class ClientSessionPool;
class ListeningPeerPool;
class AbstractRemoteRelayPeerPool;
} // namespace model

namespace controller {

class AbstractTrafficRelay;

struct ConnectToPeerRequestEx:
    api::ConnectToPeerRequest
{
    SocketAddress clientEndpoint;
};

class AbstractConnectSessionManager
{
public:
    //---------------------------------------------------------------------------------------------
    // Completion handler types.

    using CreateClientSessionHandler =
        nx::utils::MoveOnlyFunc<void(api::ResultCode, api::CreateClientSessionResponse)>;

    using ConnectToPeerHandler =
        nx::utils::MoveOnlyFunc<void(api::ResultCode, nx::network::http::ConnectionEvents)>;

    //---------------------------------------------------------------------------------------------

    virtual ~AbstractConnectSessionManager() = default;

    virtual void createClientSession(
        const api::CreateClientSessionRequest& request,
        CreateClientSessionHandler completionHandler) = 0;

    virtual void connectToPeer(
        const ConnectToPeerRequestEx& request,
        ConnectToPeerHandler completionHandler) = 0;
};

class ConnectSessionManager:
    public AbstractConnectSessionManager
{
public:
    ConnectSessionManager(
        const conf::Settings& settings,
        model::ClientSessionPool* clientSessionPool,
        relaying::ListeningPeerPool* listeningPeerPool,
        model::AbstractRemoteRelayPeerPool* remoteRelayPeerPool,
        controller::AbstractTrafficRelay* trafficRelay);
    ~ConnectSessionManager();

    virtual void createClientSession(
        const api::CreateClientSessionRequest& request,
        CreateClientSessionHandler completionHandler) override;

    virtual void connectToPeer(
        const ConnectToPeerRequestEx& request,
        ConnectToPeerHandler completionHandler) override;

private:
    struct RelaySession
    {
        std::string id;
        std::string clientPeerName;
        std::unique_ptr<AbstractStreamSocket> clientConnection;
        std::string listeningPeerName;
        std::unique_ptr<AbstractStreamSocket> listeningPeerConnection;
        nx::network::http::StringType openTunnelNotificationBuffer;
    };

    const conf::Settings& m_settings;
    model::ClientSessionPool* m_clientSessionPool;
    relaying::ListeningPeerPool* m_listeningPeerPool;
    model::AbstractRemoteRelayPeerPool* m_remoteRelayPeerPool;
    controller::AbstractTrafficRelay* m_trafficRelay;
    utils::Counter m_apiCallCounter;
    std::list<RelaySession> m_relaySessions;
    QnMutex m_mutex;
    bool m_terminated = false;

    void onAcquiredListeningPeerConnection(
        const std::string& connectSessionId,
        const std::string& listeningPeerName,
        ConnectSessionManager::ConnectToPeerHandler completionHandler,
        api::ResultCode resultCode,
        std::unique_ptr<AbstractStreamSocket> listeningPeerConnection);
    void startRelaying(
        const std::string& connectSessionId,
        const std::string& listeningPeerName,
        std::unique_ptr<AbstractStreamSocket> listeningPeerConnection,
        nx::network::http::HttpServerConnection* httpConnection);
    void startRelaying(RelaySession relaySession);
};

class ConnectSessionManagerFactory
{
public:
    using FactoryFunc = nx::utils::MoveOnlyFunc<
        std::unique_ptr<AbstractConnectSessionManager>(
            const conf::Settings& settings,
            model::ClientSessionPool* clientSessionPool,
            relaying::ListeningPeerPool* listeningPeerPool,
            controller::AbstractTrafficRelay* trafficRelay)>;

    static std::unique_ptr<AbstractConnectSessionManager> create(
        const conf::Settings& settings,
        model::ClientSessionPool* clientSessionPool,
        relaying::ListeningPeerPool* listeningPeerPool,
        model::AbstractRemoteRelayPeerPool* remoteRelayPeerPool,
        controller::AbstractTrafficRelay* trafficRelay);
    /**
     * @return Previous factory func.
     */
    static FactoryFunc setFactoryFunc(FactoryFunc func);
};

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
