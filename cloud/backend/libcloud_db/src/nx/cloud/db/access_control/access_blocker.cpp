#include "access_blocker.h"

#include <QtCore/QJsonDocument>

#include <nx/cloud/db/client/cdb_request_path.h>

#include "../settings.h"

namespace nx::cloud::db {

AccessBlocker::AccessBlocker(
    const conf::Settings& settings)
    :
    m_settings(settings),
    m_hostLockerPool(
        settings.loginEnumerationProtectionSettings(),
        std::max(
            settings.loginEnumerationProtectionSettings().period,
            settings.loginEnumerationProtectionSettings().maxBlockPeriod))
{
    if (auto userLockerSettings = settings.loginLockout())
        m_userLocker = std::make_unique<CloudUserLockerPool>(*userLockerSettings);
}

bool AccessBlocker::isBlocked(
    const nx::network::http::HttpServerConnection& connection,
    const std::string& login) const
{
    const auto userLockKey = std::make_tuple(connection.clientEndpoint().address, login);
    if (!login.empty() && m_userLocker && m_userLocker->isLocked(userLockKey))
        return true;

    return m_hostLockerPool.isLocked(connection.clientEndpoint().address);
}

void AccessBlocker::onAuthenticationSuccess(
    AuthenticationType authenticationType,
    const nx::network::http::HttpServerConnection& connection,
    std::string login,
    const nx::network::http::Request& request)
{
    if (login.empty())
        login = tryFetchLoginFromRequest(request);

    if (authenticationType == AuthenticationType::credentials)
    {
        updateUserLockoutState(
            network::server::AuthResult::success,
            connection.clientEndpoint().address,
            request,
            login);
    }
    else
    {
        // Request has not been authenticated actually.
        // So, not giving any preference to the client that issued such a request.
        // Counting it as an authentication failure to be on safe side.
        onAuthenticationFailure(authenticationType, connection, request, login);
        return;
    }

    updateHostLockoutState(
        connection.clientEndpoint().address,
        request,
        nx::network::server::AuthResult::success,
        login);
}

void AccessBlocker::onAuthenticationFailure(
    AuthenticationType authenticationType,
    const nx::network::http::HttpServerConnection& connection,
    const nx::network::http::Request request,
    const std::string& login)
{
    NX_VERBOSE(this, lm("Authentication failure. %1 - \"%2\" \"%3\"")
        .args(connection.clientEndpoint().address, login, request.requestLine.toString()));

    if (authenticationType == AuthenticationType::credentials)
    {
        updateUserLockoutState(
            network::server::AuthResult::failure,
            connection.clientEndpoint().address,
            request,
            login);
    }

    updateHostLockoutState(
        connection.clientEndpoint().address,
        request,
        nx::network::server::AuthResult::failure,
        login);
}

std::string AccessBlocker::tryFetchLoginFromRequest(
    const nx::network::http::Request& request)
{
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(request.messageBody, &error);
    if (error.error != QJsonParseError::NoError)
        return std::string();

    const auto value = document.object().value("email");
    if (value.isString())
        return value.toString().toStdString();

    return std::string();
}

void AccessBlocker::updateUserLockoutState(
    network::server::AuthResult authResult,
    const nx::network::HostAddress& host,
    const nx::network::http::Request& request,
    const std::string& login)
{
    if (!m_userLocker)
        return;

    NX_VERBOSE(this, lm("Updating user %1 lockout state. Request %2, host %3, auth result %4")
        .args(login, request.requestLine.url.path(), host, toString(authResult)));

    const auto key = std::make_tuple(host, login);
    switch (m_userLocker->updateLockoutState(
        key, authResult, request.requestLine.url.path().toStdString()))
    {
        case nx::network::server::LockUpdateResult::locked:
            NX_WARNING(this, lm("Login %1 blocked for host %2 for %3")
                .args(login, host, m_userLocker->settings().lockPeriod));
            printLockReason(*m_userLocker, key);
            break;

        case nx::network::server::LockUpdateResult::unlocked:
            NX_INFO(this, lm("Login %1 unblocked for host %2").args(login, host));
            break;

        case nx::network::server::LockUpdateResult::noChange:
            break;
    }
}

void AccessBlocker::updateHostLockoutState(
    const nx::network::HostAddress& host,
    const nx::network::http::Request request,
    nx::network::server::AuthResult authResult,
    const std::string& login)
{
    if (login.empty())
        return;

    NX_VERBOSE(this, lm("Updating host %1 lockout state. Request %2, login %3, auth result %4")
        .args(host, request.requestLine.url.path(), login, toString(authResult)));

    std::chrono::milliseconds lockPeriod{0};
    const auto result = m_hostLockerPool.updateLockoutState(
        host,
        authResult,
        request.requestLine.url.path().toStdString(),
        login,
        &lockPeriod);
    switch (result)
    {
        case nx::network::server::LockUpdateResult::locked:
            NX_WARNING(this, lm("Host %1 blocked for %2").args(host, lockPeriod));
            printLockReason(m_hostLockerPool, host);
            break;

        case nx::network::server::LockUpdateResult::unlocked:
            NX_INFO(this, lm("Host %1 unblocked").args(host));
            break;

        case nx::network::server::LockUpdateResult::noChange:
            break;
    }
}

template<typename Blocker, typename Key>
void AccessBlocker::printLockReason(const Blocker& blocker, const Key& key)
{
    using namespace std::chrono;

    const auto currentTime = nx::utils::utcTime();
    const auto currentLockReason = blocker.getCurrentLockReason(key);
    for (const auto& reason: currentLockReason)
    {
        NX_INFO(this, lm("-- %1. %2").args(
            duration_cast<milliseconds>(currentTime - reason.timestamp),
            reason.params));
    }
}

} // namespace nx::cloud::db
