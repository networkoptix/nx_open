#pragma once

#include <atomic>

#include <nx/network/http/server/proxy/proxy_handler.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/mutex.h>

#include <nx/cloud/relaying/listening_peer_pool.h>

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; }
namespace model { class RemoteRelayPeerPoolAioWrapper; }
namespace model { class AliasManager; }

namespace view {

class ProxyHandler:
    public network::http::server::proxy::AbstractProxyHandler
{
    using base_type = network::http::server::proxy::AbstractProxyHandler;

public:
    ProxyHandler(
        const conf::Settings& settings,
        relaying::AbstractListeningPeerPool* listeningPeerPool,
        model::RemoteRelayPeerPoolAioWrapper* remotePeerPool,
        model::AliasManager* aliasManager);
    ~ProxyHandler();

    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override;

protected:
    virtual void detectProxyTarget(
        const nx::network::http::HttpServerConnection& connection,
        nx::network::http::Request* const request,
        ProxyTargetDetectedHandler handler) override;

    virtual std::unique_ptr<network::aio::AbstractAsyncConnector>
        createTargetConnector() override;

private:
    relaying::AbstractListeningPeerPool* m_listeningPeerPool;
    model::RemoteRelayPeerPoolAioWrapper* m_remotePeerPool;
    ProxyTargetDetectedHandler m_detectProxyTargetHandler;
    nx::utils::Url m_requestUrl;
    nx::utils::AsyncOperationGuard m_guard;
    std::atomic<int> m_pendingRequestCount{0};
    nx::utils::MoveOnlyFunc<void(
        std::optional<std::string> /*relayHostName*/,
        std::string /*proxyTargetHostName*/)> m_findRelayInstanceHandler;
    QnMutex m_mutex;
    std::string m_targetHostAlias;
    model::AliasManager* m_aliasManager = nullptr;

    std::vector<std::string> extractTargetHostNameCandidates(
        const std::string& hostHeader) const;

    std::optional<std::string> findHostByAlias(
        const std::vector<std::string>& possibleAliases);

    void findRelayInstanceToRedirectTo(
        const std::vector<std::string>& hostNames,
        ProxyTargetDetectedHandler handler);

    std::optional<std::string> selectOnlineHostLocally(
        const std::vector<std::string>& candidates) const;

    void invokeRemotePeerPool(
        const std::vector<std::string>& hostNames,
        nx::utils::MoveOnlyFunc<void(
            std::optional<std::string> /*relayHostName*/,
            std::string /*proxyTargetHostName*/)> handler);

    void processFindRelayResult(
        const std::string& relayDomain,
        const std::string& proxyTarget);

    void onRelayInstanceSearchCompleted(
        std::optional<std::string> relayHost,
        const std::string& proxyTargetHost);
};

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
