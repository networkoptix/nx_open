#pragma once

#include <chrono>

#include <nx/network/http/http_types.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/utils/std/optional.h>

namespace nx::cdb {

namespace conf { class Settings; }

struct SecurityActions
{
    std::optional<std::chrono::milliseconds> responseDelay;
};

class TransportSecurityManager
{
public:
    TransportSecurityManager(const conf::Settings& settings);

    SecurityActions analyze(
        const nx::network::http::HttpServerConnection& connection,
        const nx::network::http::Request& request) const;

private:
    const conf::Settings& m_settings;
};

} // namespace nx::cdb
