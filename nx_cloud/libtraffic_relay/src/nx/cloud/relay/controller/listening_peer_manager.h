#pragma once

#include <nx/network/cloud/tunnel/relay/api/relay_api_data_types.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_result_code.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace cloud {
namespace relay {

namespace conf { struct ListeningPeer; };
namespace model { class ListeningPeerPool; }

namespace controller {

class AbstractListeningPeerManager
{
public:
    using BeginListeningHandler =
        nx::utils::MoveOnlyFunc<void(
            api::ResultCode, api::BeginListeningResponse, nx_http::ConnectionEvents)>;

    virtual ~AbstractListeningPeerManager() = default;

    virtual void beginListening(
        const api::BeginListeningRequest& request,
        BeginListeningHandler completionHandler) = 0;
};

//-------------------------------------------------------------------------------------------------

class ListeningPeerManager:
    public AbstractListeningPeerManager
{
public:
    ListeningPeerManager(
        const conf::ListeningPeer& settings,
        model::ListeningPeerPool* listeningPeerPool);

    virtual void beginListening(
        const api::BeginListeningRequest& request,
        BeginListeningHandler completionHandler) override;

private:
    const conf::ListeningPeer& m_settings;
    model::ListeningPeerPool* m_listeningPeerPool;

    void saveServerConnection(
        const std::string& peerName,
        nx_http::HttpServerConnection* httpConnection);
};

//-------------------------------------------------------------------------------------------------

using ListeningPeerManagerFactoryFunction =
    std::unique_ptr<AbstractListeningPeerManager>(
        const conf::ListeningPeer&,
        model::ListeningPeerPool*);

class ListeningPeerManagerFactory:
    public nx::utils::BasicFactory<ListeningPeerManagerFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<ListeningPeerManagerFactoryFunction>;

public:
    ListeningPeerManagerFactory();

    static ListeningPeerManagerFactory& instance();

private:
    std::unique_ptr<AbstractListeningPeerManager> defaultFactoryFunction(
        const conf::ListeningPeer&,
        model::ListeningPeerPool*);
};

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
