#pragma once

#include <memory>

#include <nx/network/cloud/tunnel/relay/api/relay_api_data_types.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_result_code.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/utils/counter.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; };

namespace model {
class ClientSessionPool;
class ListeningPeerPool;
} // namespace model

namespace controller {

class AbstractTrafficRelay;

class AbstractConnectSessionManager
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

class ConnectSessionManager:
    public AbstractConnectSessionManager
{
public:
    ConnectSessionManager(
        const conf::Settings& settings,
        model::ClientSessionPool* clientSessionPool,
        model::ListeningPeerPool* listeningPeerPool,
        controller::AbstractTrafficRelay* trafficRelay);
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
    const conf::Settings& m_settings;
    model::ClientSessionPool* m_clientSessionPool;
    model::ListeningPeerPool* m_listeningPeerPool;
    controller::AbstractTrafficRelay* m_trafficRelay;
    utils::Counter m_apiCallCounter;

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
        const std::string& clientSessionId,
        const std::string& listeningPeerName,
        std::unique_ptr<AbstractStreamSocket> listeningPeerConnection,
        nx_http::HttpServerConnection* httpConnection);
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
