#pragma once

#include <string>

#include <nx/network/http/tunneling/client.h>
#include <nx/utils/url.h>

#include "abstract_transaction_transport_connector.h"

namespace nx::clusterdb::engine::transport {

class HttpTunnelTransportConnector:
    public AbstractTransactionTransportConnector
{
    using base_type = AbstractTransactionTransportConnector;

public:
    HttpTunnelTransportConnector(
        const std::string& systemId,
        const std::string& connectionId,
        const nx::utils::Url& targetUrl);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void connect(Handler completionHandler) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const std::string m_systemId;
    const std::string m_connectionId;
    const nx::utils::Url m_targetUrl;
    std::unique_ptr<nx::network::http::tunneling::Client> m_client;
    Handler m_completionHandler;

    void processOpenTunnelResult(
        nx::network::http::tunneling::OpenTunnelResult result);
};

} // namespace nx::clusterdb::engine::transport
