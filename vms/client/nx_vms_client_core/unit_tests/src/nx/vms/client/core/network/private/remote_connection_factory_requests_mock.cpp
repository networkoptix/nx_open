// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_factory_requests_mock.h"

namespace nx::vms::client::core {
namespace test {

using UserType = nx::vms::api::UserType;

namespace {

std::string makeAccessToken(const std::string& refreshToken, const QString& cloudSystemId)
{
    return (QString::fromStdString(refreshToken) + ":" + cloudSystemId).toStdString();
}

} // namespace

RemoteConnectionFactoryRequestsManager::ModuleInformationReply
    RemoteConnectionFactoryRequestsManager::getModuleInformation(ContextPtr context) const
{
    ++m_requestsCount;
    ModuleInformationReply result{.handshakeCertificateChain = m_handshakeCertificateChain};
    if (!m_servers.empty())
        result.moduleInformation = m_servers[0];

    return result;;
}

RemoteConnectionFactoryRequestsManager::ServersInfoReply
    RemoteConnectionFactoryRequestsManager::getServersInfo(ContextPtr context) const
{
    ++m_requestsCount;
    return {.serversInfo = m_servers, .handshakeCertificateChain = m_handshakeCertificateChain};
}

nx::vms::api::UserModelV1
    RemoteConnectionFactoryRequestsManager::getUserModel(ContextPtr context) const
{
    ++m_requestsCount;
    return {};
}

nx::vms::api::LoginUser RemoteConnectionFactoryRequestsManager::getUserType(
    ContextPtr context) const
{
    ++m_requestsCount;
    return {};
}

nx::vms::api::LoginSession RemoteConnectionFactoryRequestsManager::createLocalSession(
    ContextPtr context) const
{
    ++m_requestsCount;
    auto credentials = context->credentials();
    if (!credentials.authToken.isPassword())
    {
        context->setError(RemoteConnectionErrorCode::forbiddenRequest);
        return {};
    }

    for (const auto& user: m_users)
    {
        if (user.password == credentials.authToken.value)
            return {.username = QString::fromStdString(user.username), .token = user.token};
    }

    context->setError(RemoteConnectionErrorCode::unauthorized);
    return {};
}

nx::vms::api::LoginSession RemoteConnectionFactoryRequestsManager::createTemporaryLocalSession(
    ContextPtr context) const
{
    return createLocalSession(context);
}

nx::vms::api::LoginSession RemoteConnectionFactoryRequestsManager::getCurrentSession(
    ContextPtr context) const
{
    ++m_requestsCount;
    auto credentials = context->credentials();
    if (!credentials.authToken.isBearerToken())
    {
        context->setError(RemoteConnectionErrorCode::forbiddenRequest);
        return {};
    }

    const QString cloudSystemId = m_servers.empty()
        ? QString()
        : m_servers[0].cloudSystemId;

    for (const auto& user: m_users)
    {
        if (user.token == credentials.authToken.value)
        {
            if (user.userType == UserType::local)
                return {.username = QString::fromStdString(user.username), .token = user.token};

            context->setError(RemoteConnectionErrorCode::unauthorized);
            return {};
        }

        if ((user.userType == UserType::cloud
            && makeAccessToken(user.token, cloudSystemId) == credentials.authToken.value))
        {
            return {
                .username = QString::fromStdString(user.username),
                .token = credentials.authToken.value
            };
        }
    }

    context->setError(RemoteConnectionErrorCode::sessionExpired);
    return {};
}

void RemoteConnectionFactoryRequestsManager::checkDigestAuthentication(
    ContextPtr context, bool is2FaEnabledForUser) const
{
    ++m_requestsCount;
    auto credentials = context->credentials();

    auto user = std::find_if(m_users.cbegin(), m_users.cend(),
        [name = credentials.username](const auto& user) { return user.username == name; });

    if (user == m_users.cend())
    {
        context->setError(RemoteConnectionErrorCode::unauthorized);
        return;
    }

    if (!credentials.authToken.isPassword() || credentials.authToken.value != user->password)
    {
        context->setError(RemoteConnectionErrorCode::unauthorized);
        return;
    }
}

std::future<RemoteConnectionFactoryContext::CloudTokenInfo>
    RemoteConnectionFactoryRequestsManager::issueCloudToken(
        ContextPtr context,
        const QString& cloudSystemId) const
{
    auto credentials = context->credentials();
    return std::async(
        [this, credentials, cloudSystemId]() -> Context::CloudTokenInfo
        {
            if (!credentials.authToken.isBearerToken())
                return {.error = RemoteConnectionErrorCode::unauthorized};

            auto user = std::find_if(m_users.cbegin(), m_users.cend(),
                [name = credentials.username](const auto& user) { return user.username == name; });

            if (user == m_users.cend())
                return {.error = RemoteConnectionErrorCode::unauthorized};

            if (user->token != credentials.authToken.value)
                return {.error = RemoteConnectionErrorCode::unauthorized};

            Context::CloudTokenInfo result;
            result.response.access_token = makeAccessToken(user->token, cloudSystemId);
            return result;
        });
}

} // namespace test
} // namespace nx::vms::client::core
