#pragma once

#include <ostream>

#include <nx/network/http/test_http_server.h>
#include <nx/utils/argument_parser.h>

namespace nx {
namespace cctu {

void printListenOnRelayOptions(std::ostream* output);

int runInListenOnRelayMode(const nx::utils::ArgumentParser& args);

//-------------------------------------------------------------------------------------------------

class ListenOnRelaySettings
{
public:
    ListenOnRelaySettings(const nx::utils::ArgumentParser& args);

    nx::utils::Url baseRelayUrl() const;
    std::string listeningPeerHostName() const;

private:
    nx::utils::Url m_baseRelayUrl;
    std::string m_listeningPeerHostName;
};

//-------------------------------------------------------------------------------------------------

class TestHttpServerOnProxy
{
public:
    TestHttpServerOnProxy(const ListenOnRelaySettings& settings);

private:
    std::unique_ptr<nx::network::http::TestHttpServer> m_httpServer;

    void httpRequestHandler(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request request,
        nx::network::http::Response* const /*response*/,
        nx::network::http::RequestProcessedHandler completionHandler);
};

} // namespace cctu
} // namespace nx
