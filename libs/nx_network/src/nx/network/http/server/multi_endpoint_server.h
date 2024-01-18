// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/server/http_stream_socket_server.h>

#include "https_server_context.h"
#include "settings.h"

namespace nx::network::http::server {

namespace rest { class MessageDispatcher; }

class AbstractAuthenticationManager;

// NOTE: This class is implemented in the header because exporting it leads to some weird link error
// on mswin. TODO: #akolesnikov try exporting it after switch to vs2019.

/**
 * Listens for incoming HTTP connections on multiple local endpoints.
 */
class MultiEndpointServer:
    public nx::network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>,
    public AbstractHttpStatisticsProvider
{
    using base_type =
        nx::network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>;

public:
    template<typename... Args>
    MultiEndpointServer(
        AbstractRequestHandler* requestHandler,
        Args&&... args)
        :
        base_type(
            requestHandler,
            std::forward<Args>(args)...)
    {
    }

    virtual network::http::server::HttpStatistics httpStatistics() const override
    {
        return m_httpStatsProvider
            ? m_httpStatsProvider->httpStatistics()
            : network::http::server::HttpStatistics();
    }

    void setTcpBackLogSize(int tcpBackLogSize)
    {
        m_tcpBackLogSize = tcpBackLogSize;
    }

    void setExtraResponseHeaders(HttpHeaders responseHeaders)
    {
        m_extraResponseHeaders = std::move(responseHeaders);
    }

    bool listen()
    {
        initializeHttpStatisticsProvider();
        propagateExtraResponseHeaders();

        return base_type::listen(m_tcpBackLogSize);
    }

    void append(std::unique_ptr<MultiEndpointServer> server)
    {
        m_urls.insert(m_urls.end(), server->m_urls.begin(), server->m_urls.end());

        base_type::append(std::move(server));
    }

    /**
     * @return List of URLs containing scheme and endpoint only.
     */
    const std::vector<nx::utils::Url>& urls() const
    {
        return m_urls;
    }

    // TODO: #akolesnikov Remove this function! URL list should be auto-generated.
    // Though, this requires some refactoring in some nx::network::server classes.
    // Relying on Builder to set this properly.
    void setUrls(std::vector<nx::utils::Url> urls)
    {
        m_urls = std::move(urls);
    }

    /**
     * @return HTTPS url (if any), otherwise - any other URL.
     */
    nx::utils::Url preferredUrl() const
    {
        auto it = std::find_if(
            m_urls.begin(), m_urls.end(),
            [](const auto& url) { return url.scheme() == network::http::kSecureUrlSchemeName; });

        return it != m_urls.end() ? *it : m_urls.front();
    }

private:
    int m_tcpBackLogSize = Settings::kDefaultTcpBacklogSize;
    std::vector<nx::utils::Url> m_urls;
    std::unique_ptr<SummingStatisticsProvider> m_httpStatsProvider;
    HttpHeaders m_extraResponseHeaders;

    void initializeHttpStatisticsProvider()
    {
        std::vector<const AbstractHttpStatisticsProvider*> providers;
        forEachListener(
            [&providers](const auto& listener)
            {
                providers.emplace_back(listener);
            });

        m_httpStatsProvider =
            std::make_unique<SummingStatisticsProvider>(std::move(providers));
    }

    void propagateExtraResponseHeaders()
    {
        forEachListener(
            [this](HttpStreamSocketServer* server)
            {
                server->setExtraResponseHeaders(m_extraResponseHeaders);
            });
    }
};

//-------------------------------------------------------------------------------------------------

class MultiEndpointSslServer:
    public MultiEndpointServer
{
    using base_type = MultiEndpointServer;

public:
    MultiEndpointSslServer(
        AbstractRequestHandler* requestHandler,
        std::unique_ptr<HttpsServerContext> httpsContext)
        :
        base_type(
            requestHandler,
            httpsContext ? &httpsContext->context() : ssl::Context::instance()),
        m_httpsContext(std::move(httpsContext))
    {
    }

    virtual ~MultiEndpointSslServer() override
    {
        listeners().clear();
    }

private:
    std::unique_ptr<HttpsServerContext> m_httpsContext;
};

} // namespace nx::network::http::server
