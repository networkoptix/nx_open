#pragma once

#include <nx/network/cloud/tunnel/relay/api/relay_api_data_types.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_result_code.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace cloud {
namespace relaying {

class ListeningPeerPool;
struct Settings;

class NX_RELAYING_API AbstractListeningPeerManager
{
public:
    using BeginListeningHandler =
        nx::utils::MoveOnlyFunc<void(
            relay::api::ResultCode, relay::api::BeginListeningResponse, nx::network::http::ConnectionEvents)>;

    virtual ~AbstractListeningPeerManager() = default;

    virtual void beginListening(
        const relay::api::BeginListeningRequest& request,
        BeginListeningHandler completionHandler) = 0;
};

//-------------------------------------------------------------------------------------------------

class NX_RELAYING_API ListeningPeerManager:
    public AbstractListeningPeerManager
{
public:
    ListeningPeerManager(
        const Settings& settings,
        ListeningPeerPool* listeningPeerPool);

    virtual void beginListening(
        const relay::api::BeginListeningRequest& request,
        BeginListeningHandler completionHandler) override;

private:
    const Settings& m_settings;
    ListeningPeerPool* m_listeningPeerPool;

    void saveServerConnection(
        const std::string& peerName,
        nx::network::http::HttpServerConnection* httpConnection);
};

//-------------------------------------------------------------------------------------------------

using ListeningPeerManagerFactoryFunction =
    std::unique_ptr<AbstractListeningPeerManager>(
        const Settings&,
        ListeningPeerPool*);

class NX_RELAYING_API ListeningPeerManagerFactory:
    public nx::utils::BasicFactory<ListeningPeerManagerFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<ListeningPeerManagerFactoryFunction>;

public:
    ListeningPeerManagerFactory();

    static ListeningPeerManagerFactory& instance();

private:
    std::unique_ptr<AbstractListeningPeerManager> defaultFactoryFunction(
        const Settings&,
        ListeningPeerPool*);
};

} // namespace relaying
} // namespace cloud
} // namespace nx
