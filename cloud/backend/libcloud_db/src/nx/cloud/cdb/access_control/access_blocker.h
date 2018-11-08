#pragma once

#include <chrono>
#include <map>

#include <nx/network/connection_server/access_blocker_pool.h>
#include <nx/network/connection_server/user_locker.h>
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
        const std::string& login) const;

    void onAuthenticationSuccess(
        AuthenticationType authenticationType,
        const nx::network::http::HttpServerConnection& connection,
        std::string login,
        const nx::network::http::Request& request);

    void onAuthenticationFailure(
        AuthenticationType authenticationType,
        const nx::network::http::HttpServerConnection& connection,
        const std::string& login);

private:
    using LoginEnumerationProtectorPool = 
        nx::network::server::AccessBlockerPool<
            nx::network::HostAddress,
            LoginEnumerationProtector,
            LoginEnumerationProtectionSettings>;

    const conf::Settings& m_settings;
    LoginEnumerationProtectorPool m_hostLockerPool;
    std::unique_ptr<network::server::UserLockerPool> m_userLocker;

    std::string tryFetchLoginFromRequest(const nx::network::http::Request& request);

    void updateUserLockoutState(
        network::server::AuthResult authResult,
        const nx::network::HostAddress& host,
        const std::string& login);

    void updateHostLockoutState(
        const nx::network::HostAddress& host,
        nx::network::server::AuthResult authenticationResult,
        const std::string& login);
};

} // namespace nx::cdb
