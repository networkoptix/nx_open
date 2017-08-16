#pragma once

#include <memory>
#include <list>

#include <nx/network/cloud/tunnel/relay/api/relay_api_data_types.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_result_code.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/utils/counter.h>
#include <nx/utils/subscription.h>
#include <nx/utils/thread/cf/async_queued_executor.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

#include "abstract_listen_address_helper_handler.h"

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

class AbstractConnectSessionManager: public AbstractListenAddressHelperHandler
{
public:
    //---------------------------------------------------------------------------------------------
    // Completion handler types.

    using BeginListeningHandler =
        nx::utils::MoveOnlyFunc<void(
            api::ResultCode, api::BeginListeningResponse, nx_http::ConnectionEvents)>;

    using CreateClientSessionHandler =
        nx::utils::MoveOnlyFunc<void(api::ResultCode, api::CreateClientSessionResponse)>;

    using ConnectToPeerHandler =
        nx::utils::MoveOnlyFunc<void(api::ResultCode, nx_http::ConnectionEvents)>;

    //---------------------------------------------------------------------------------------------

    virtual ~AbstractConnectSessionManager() = default;

    virtual void beginListening(
        const api::BeginListeningRequest& request,
        BeginListeningHandler completionHandler) = 0;

    virtual void createClientSession(
        const api::CreateClientSessionRequest& request,
        CreateClientSessionHandler completionHandler) = 0;

    virtual void connectToPeer(
        const api::ConnectToPeerRequest& request,
        ConnectToPeerHandler completionHandler) = 0;
};

class ConnectSessionManager: public AbstractConnectSessionManager
{
public:
    ConnectSessionManager(
        const conf::Settings& settings,
        model::ClientSessionPool* clientSessionPool,
        model::ListeningPeerPool* listeningPeerPool,
        controller::AbstractTrafficRelay* trafficRelay,
        std::unique_ptr<model::AbstractRemoteRelayPeerPool> remoteRelayPool);
    ~ConnectSessionManager();

    virtual void beginListening(
        const api::BeginListeningRequest& request,
        BeginListeningHandler completionHandler) override;

    virtual void createClientSession(
        const api::CreateClientSessionRequest& request,
        CreateClientSessionHandler completionHandler) override;

    virtual void connectToPeer(
        const api::ConnectToPeerRequest& request,
        ConnectToPeerHandler completionHandler) override;

private:
    struct RelaySession
    {
        std::string id;
        std::string clientPeerName;
        std::unique_ptr<AbstractStreamSocket> clientConnection;
        std::string listeningPeerName;
        std::unique_ptr<AbstractStreamSocket> listeningPeerConnection;
        nx_http::StringType openTunnelNotificationBuffer;
    };

    const conf::Settings& m_settings;
    model::ClientSessionPool* m_clientSessionPool;
    model::ListeningPeerPool* m_listeningPeerPool;
    controller::AbstractTrafficRelay* m_trafficRelay;
    utils::Counter m_apiCallCounter;
    std::list<RelaySession> m_relaySessions;
    QnMutex m_mutex;
    bool m_terminated = false;
    std::unique_ptr<model::AbstractRemoteRelayPeerPool> m_remoteRelayPool;
    std::set<nx::utils::SubscriptionId> m_listeningPeerPoolSubscriptions;

    void saveServerConnection(
        const std::string& peerName,
        nx_http::HttpServerConnection* httpConnection);

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
        nx_http::HttpServerConnection* httpConnection);
    void sendOpenTunnelNotification(
        std::list<RelaySession>::iterator relaySessionIter);
    void onOpenTunnelNotificationSent(
        SystemError::ErrorCode sysErrorCode,
        std::size_t bytesSent,
        std::list<RelaySession>::iterator relaySessionIter);
    void startRelaying(RelaySession relaySession);

    virtual void onPublicAddressDiscovered(std::string publicAddress) override;
    void subscribeForPeerConnected(
        nx::utils::SubscriptionId* subscriptionId,
        std::string publicAddress);
    void subscribeForPeerDisconnected(nx::utils::SubscriptionId* subscriptionId);
};

class ConnectSessionManagerFactory
{
public:
    using FactoryFunc = nx::utils::MoveOnlyFunc<
        std::unique_ptr<AbstractConnectSessionManager>(
            const conf::Settings& settings,
            model::ClientSessionPool* clientSessionPool,
            model::ListeningPeerPool* listeningPeerPool,
            controller::AbstractTrafficRelay* trafficRelay)>;

    static std::unique_ptr<AbstractConnectSessionManager> create(
        const conf::Settings& settings,
        model::ClientSessionPool* clientSessionPool,
        model::ListeningPeerPool* listeningPeerPool,
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
