#pragma once

#include <nx/utils/move_only_func.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_data_types.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_result_code.h>
#include <nx/network/http/server/http_server_connection.h>

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; };

namespace model {
class ClientSessionPool;
class ListeningPeerPool;
} // namespace model

namespace controller {

class ConnectSessionManager
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
    // Methods.

    ConnectSessionManager(
        const conf::Settings& settings,
        model::ClientSessionPool* clientSessionPool,
        model::ListeningPeerPool* listeningPeerPool);

    void beginListening(
        const api::BeginListeningRequest& request,
        BeginListeningHandler completionHandler);

    void createClientSession(
        const api::CreateClientSessionRequest& request,
        CreateClientSessionHandler completionHandler);

    void connectToPeer(
        const api::ConnectToPeerRequest& request,
        ConnectToPeerHandler completionHandler);

private:
    const conf::Settings& m_settings;
    model::ClientSessionPool* m_clientSessionPool;
    model::ListeningPeerPool* m_listeningPeerPool;

    void saveServerConnection(
        const std::string& peerName,
        nx_http::HttpServerConnection* httpConnection);
};

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
