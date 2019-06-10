#include "access_blocker.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

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
        m_userLocker = std::make_unique<network::server::UserLockerPool>(*userLockerSettings);
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
            login);
    }
    else
    {
        // Request has not been authenticated actually.
        // So, not giving any preference to the client that issued such a request.
        // Counting it as an authentication failure to be on safe side.
        onAuthenticationFailure(authenticationType, connection, login);
        return;
    }

    updateHostLockoutState(
        connection.clientEndpoint().address,
        nx::network::server::AuthResult::success,
        login);
}

void AccessBlocker::onAuthenticationFailure(
    AuthenticationType authenticationType,
    const nx::network::http::HttpServerConnection& connection,
    const std::string& login)
{
    if (authenticationType == AuthenticationType::credentials)
    {
        updateUserLockoutState(
            network::server::AuthResult::failure,
            connection.clientEndpoint().address,
            login);
    }

    updateHostLockoutState(
        connection.clientEndpoint().address,
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
    const std::string& login)
{
    if (!m_userLocker)
        return;

    switch (m_userLocker->updateLockoutState(
        std::make_tuple(host, login),
        authResult))
    {
        case nx::network::server::LockUpdateResult::locked:
            NX_WARNING(this, lm("Login %1 blocked for host %2 for %3")
                .args(login, host, m_userLocker->settings().lockPeriod));
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
    nx::network::server::AuthResult authenticationResult,
    const std::string& login)
{
    if (login.empty())
        return;

    std::chrono::milliseconds lockPeriod{0};
    const auto result = m_hostLockerPool.updateLockoutState(
        host,
        authenticationResult,
        login,
        &lockPeriod);
    switch (result)
    {
        case nx::network::server::LockUpdateResult::locked:
            NX_WARNING(this, lm("Host %1 blocked for %2").args(host, lockPeriod));
            break;

        case nx::network::server::LockUpdateResult::unlocked:
            NX_INFO(this, lm("Host %1 unblocked").args(host));
            break;

        case nx::network::server::LockUpdateResult::noChange:
            break;
    }
}

} // namespace nx::cloud::db
