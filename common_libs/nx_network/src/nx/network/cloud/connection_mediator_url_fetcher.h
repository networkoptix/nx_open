#pragma once

#include "basic_cloud_module_url_fetcher.h"

namespace nx {
namespace network {
namespace cloud {

using MediatorUrlHandler = nx::utils::MoveOnlyFunc<void(
    nx_http::StatusCode::Value /*result code*/,
    QUrl /*tcp url*/,
    QUrl /*udp url*/)>;

class NX_NETWORK_API ConnectionMediatorUrlFetcher:
    public BasicCloudModuleUrlFetcher<MediatorUrlHandler>
{
    using base_type = BasicCloudModuleUrlFetcher<MediatorUrlHandler>;

public:
    using Handler = MediatorUrlHandler;

    /**
     * Retrieves endpoint if unknown.
     * If endpoint is known, then calls handler directly from this method.
     */
    void get(nx_http::AuthInfo auth, Handler handler);
    void get(Handler handler);

protected:
    virtual bool analyzeXmlSearchResult(
        const nx::utils::stree::ResourceContainer& searchResult) override;
    virtual void invokeHandler(
        const Handler& handler,
        nx_http::StatusCode::Value statusCode) override;

private:
    struct Mediator
    {
        QUrl tcpUrl;
        QUrl udpUrl;
    };

    boost::optional<Mediator> m_mediator;
};

} // namespace cloud
} // namespace network
} // namespace nx
