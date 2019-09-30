#include "cloud_db_emulator.h"


#include <nx/cloud/db/client/cdb_request_path.h>
#include <nx/cloud/db/client/data/auth_data.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_types.h>
#include <nx/network/url/url_builder.h>

#include <nx/fusion/model_functions.h>

namespace nx::cloud::storage::service::test {

using namespace nx::network::http;
using namespace nx::cloud::db::api;

CloudDbEmulator::Account::Account(
    CloudDbEmulator* cloudDb,
    const Credentials& credentials):
    m_cloudDb(cloudDb),
    m_credentials(credentials)
{
}

CloudDbEmulator::Account& CloudDbEmulator::Account::addSystem(const std::string& systemId)
{
    m_systems[systemId] = SystemAccessRole::owner;
    return *this;
}

void CloudDbEmulator::Account::shareSystem(
    const std::string& systemId,
    const std::string& user,
    SystemAccessRole accessLevel)
{
    {
        auto it = m_cloudDb->m_accounts.find(m_credentials.username.toStdString());
        NX_ASSERT(it != m_cloudDb->m_accounts.end());
        NX_ASSERT(m_systems.find(systemId) != m_systems.end());
    }

    auto userToShareWithIter = m_cloudDb->m_accounts.find(user);
    NX_ASSERT(userToShareWithIter != m_cloudDb->m_accounts.end());

    userToShareWithIter->second.m_systems[systemId] = accessLevel;
}

const network::http::Credentials& CloudDbEmulator::Account::credentials() const
{
    return m_credentials;
}

CloudDbEmulator::CloudDbEmulator()
{
    registerHttpApi();
}

bool CloudDbEmulator::bindAndListen()
{
    return m_server.bindAndListen();
}

nx::utils::Url CloudDbEmulator::url() const
{
    return network::url::Builder()
        .setScheme(network::http::kUrlSchemeName)
        .setEndpoint(m_server.serverAddress());
}

CloudDbEmulator::Account& CloudDbEmulator::addAccount(
    const network::http::Credentials& credentials)
{
    auto owner = credentials.username.toStdString();
    return m_accounts.emplace(owner, Account(this, credentials)).first->second;
}

void CloudDbEmulator::registerHttpApi()
{
    using namespace std::placeholders;

    m_server.httpMessageDispatcher().registerRequestProcessorFunc(
        Method::post,
        cloud::db::kAuthResolveUserCredentials,
        std::bind(&CloudDbEmulator::resolveUserDigest, this, _1, _2));

    m_server.httpMessageDispatcher().registerRequestProcessorFunc(
        Method::post,
        cloud::db::kAuthSystemAccessLevel,
        std::bind(&CloudDbEmulator::getSystemAccessLevel, this, _1, _2));
}

void CloudDbEmulator::resolveUserDigest(
    RequestContext requestContext,
    RequestProcessedHandler completionHandler)
{
    auto owner = validateRequest(requestContext);
    if (!owner)
        return completionHandler(StatusCode::badRequest);

    RequestResult result(StatusCode::ok);
    CredentialsDescriptor credentials;

    if (m_accounts.find(*owner) == m_accounts.end())
    {
        credentials.status = ResultCode::notFound;
        result.dataSource = std::make_unique<BufferSource>(
            "application/json",
            QJson::serialized(credentials));
        return completionHandler(std::move(result));
    }

    credentials.objectId = std::move(*owner);
    credentials.objectType = ObjectType::account;
    credentials.status = ResultCode::ok;

    result.dataSource = std::make_unique<BufferSource>(
        "application/json",
        QJson::serialized(credentials));
    completionHandler(std::move(result));
}

void CloudDbEmulator::getSystemAccessLevel(
    RequestContext requestContext,
    RequestProcessedHandler completionHandler)
{
    auto owner = validateRequest(requestContext);
    if (!owner)
        return completionHandler(StatusCode::badRequest);

    auto systemId = requestContext.requestPathParams.getByName("systemId");
    if (systemId.empty())
        return completionHandler(StatusCode::badRequest);

    auto systemAccess = getSystemAccess(*owner, systemId);
    if (!systemAccess)
        return completionHandler(StatusCode::notFound);

    RequestResult result(StatusCode::ok);
    result.dataSource = std::make_unique<BufferSource>(
        "application/json",
        QJson::serialized(*systemAccess));
    completionHandler(std::move(result));
}

std::optional<std::string /*owner*/> CloudDbEmulator::validateRequest(
    const network::http::RequestContext& requestContext)
{
    bool ok = false;
    auto userAuth =
        QJson::deserialized(requestContext.request.messageBody, UserAuthorization(), &ok);
    if (!ok)
        return std::nullopt;

    if (userAuth.requestAuthorization.empty() || userAuth.requestMethod.empty())
        return std::nullopt;

    header::Authorization auth;
    if (!auth.parse(userAuth.requestAuthorization.c_str()))
        return std::nullopt;

    return auth.userid().toStdString();
}

std::optional<SystemAccess> CloudDbEmulator::getSystemAccess(
    const std::string& user,
    const std::string& systemId)
{
    auto userIt = m_accounts.find(user);
    if (userIt == m_accounts.end())
        return std::nullopt;

    auto systemIt = userIt->second.m_systems.find(systemId);
    if (systemIt == userIt->second.m_systems.end())
        return std::nullopt;

    return SystemAccess{systemIt->second};
}

} // namespace nx::cloud::storage::service::test
