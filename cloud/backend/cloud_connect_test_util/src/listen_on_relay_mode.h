#pragma once

#include <ostream>

#include <nx/network/http/test_http_server.h>
#include <nx/utils/argument_parser.h>

#include "http_server.h"

namespace nx {
namespace cctu {

void printListenOnRelayOptions(std::ostream* output);

int runInListenOnRelayMode(const nx::utils::ArgumentParser& args);

//-------------------------------------------------------------------------------------------------

class ListenOnRelaySettings
{
public:
    ListenOnRelaySettings() = default;
    ListenOnRelaySettings(const nx::utils::ArgumentParser& args);

    nx::utils::Url baseRelayUrl;
    std::string listeningPeerHostName;
};

//-------------------------------------------------------------------------------------------------

class TestHttpServerOnProxy
{
public:
    TestHttpServerOnProxy(const ListenOnRelaySettings& settings);

private:
    std::unique_ptr<HttpServer> m_httpServer;
};

} // namespace cctu
} // namespace nx
