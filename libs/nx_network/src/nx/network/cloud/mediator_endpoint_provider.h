#pragma once

#include <string>
#include <vector>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/url.h>

#include "connection_mediator_url_fetcher.h"

namespace nx::hpm::api {

struct MediatorAddress
{
    /**
     * Can be either stun://, http:// or https://
     */
    nx::utils::Url tcpUrl;
    network::SocketAddress stunUdpEndpoint;

    std::string toString() const
    {
        return lm("tcp URL: %1, stun udp endpoint: %2")
            .args(tcpUrl, stunUdpEndpoint).toStdString();
    }

    bool operator==(const MediatorAddress& right) const
    {
        return tcpUrl == right.tcpUrl
            && stunUdpEndpoint == right.stunUdpEndpoint;
    }
};

//-------------------------------------------------------------------------------------------------

class AbstractMediatorEndpointProvider:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    using FetchMediatorEndpointsCompletionHandler =
        nx::utils::MoveOnlyFunc<void(
            nx::network::http::StatusCode::Value /*resultCode*/)>;

    /**
     * After successful completion of this method, mediator address can be received
     * with AbstractMediatorEndpointProvider::mediatorAddress().
     */
    virtual void fetchMediatorEndpoints(
        FetchMediatorEndpointsCompletionHandler handler) = 0;

    virtual std::optional<MediatorAddress> mediatorAddress() const = 0;
};

//-------------------------------------------------------------------------------------------------

// TODO: #ak Merge this class with ConnectionMediatorUrlFetcher.
class MediatorEndpointProvider:
    public AbstractMediatorEndpointProvider
{
    using base_type = AbstractMediatorEndpointProvider;

public:
    MediatorEndpointProvider(const std::string& cloudHost);

    virtual void bindToAioThread(
        network::aio::AbstractAioThread* aioThread) override;

    void mockupCloudModulesXmlUrl(const nx::utils::Url& cloudModulesXmlUrl);

    void mockupMediatorAddress(const MediatorAddress& mediatorAddress);

    virtual void fetchMediatorEndpoints(
        FetchMediatorEndpointsCompletionHandler handler) override;

    virtual std::optional<MediatorAddress> mediatorAddress() const override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const std::string m_cloudHost;
    mutable QnMutex m_mutex;
    std::vector<FetchMediatorEndpointsCompletionHandler> m_fetchMediatorEndpointsHandlers;
    std::unique_ptr<nx::network::cloud::ConnectionMediatorUrlFetcher> m_mediatorUrlFetcher;
    std::optional<nx::utils::Url> m_cloudModulesXmlUrl;
    std::optional<MediatorAddress> m_mediatorAddress;

    void initializeUrlFetcher();
};

} // namespace nx::hpm::api
