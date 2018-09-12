#pragma once

#include <chrono>
#include <map>

#include <nx/network/connection_server/access_blocker_pool.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/mutex.h>

#include "auth_types.h"
#include "login_enumeration_protector.h"

namespace nx::cdb {

namespace conf { class Settings; }

/**
 * Can block access based on different rules.
 * Used to prevent different kinds of "enumeration" attacks.
 */
class AccessBlocker
{
public:
    AccessBlocker(const conf::Settings& settings);

    bool isBlocked(
        const nx::network::http::HttpServerConnection& connection,
        const std::string& login,
        const nx::network::http::Request& request) const;

    void onAuthenticationSuccess(
        AuthenticationType authenticationType,
        const nx::network::http::HttpServerConnection& connection,
        std::string login,
        const nx::network::http::Request& request);

    void onAuthenticationFailure(
        const nx::network::http::HttpServerConnection& connection,
        const std::string& login,
        const nx::network::http::Request& request);

private:
    using LoginEnumerationProtectorPool = 
        nx::network::server::AccessBlockerPool<
            nx::network::HostAddress,
            LoginEnumerationProtector,
            LoginEnumerationProtectionSettings>;

    const conf::Settings& m_settings;
    LoginEnumerationProtectorPool m_hostLockerPool;

    std::string tryFetchLoginFromRequest(const nx::network::http::Request& request);
};

} // namespace nx::cdb
