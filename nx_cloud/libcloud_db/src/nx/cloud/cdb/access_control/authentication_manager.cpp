#include "authentication_manager.h"

#include <algorithm>
#include <future>
#include <limits>

#include <nx/cloud/cdb/client/data/types.h>

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/utils/cryptographic_random_device.h>
#include <nx/utils/random.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/time.h>

#include "abstract_authentication_data_provider.h"
#include "../managers/account_manager.h"
#include "../managers/temporary_account_password_manager.h"
#include "../managers/system_manager.h"
#include "../stree/cdb_ns.h"
#include "../stree/http_request_attr_reader.h"
#include "../stree/socket_attr_reader.h"
#include "../stree/stree_manager.h"

namespace nx {
namespace cdb {

using namespace nx::network::http;

namespace detail {

AuthenticationHelper::AuthenticationHelper(
    const nx::network::http::AuthMethodRestrictionList& authRestrictionList,
    network::server::UserLockerPool* userLocker,
    const nx::network::http::HttpServerConnection& connection,
    const nx::network::http::Request& request,
    nx::network::http::server::AuthenticationCompletionHandler handler)
    :
    m_authRestrictionList(authRestrictionList),
    m_userLocker(userLocker),
    m_connection(connection),
    m_request(request),
    m_handler(std::move(handler))
{
    loadAuthorizationHeader();

    m_username = m_authzHeader
        ? m_authzHeader->userid().toStdString()
        : std::string();

    m_userLockKey = std::make_tuple(
        connection.clientEndpoint().address,
        m_username);

    if (m_authzHeader)
    {
        m_validateHa1Func = std::bind(
            &validateAuthorization,
            request.requestLine.method,
            m_username.c_str(),
            boost::none,
            std::placeholders::_1,
            *m_authzHeader);
    }
}

const std::string& AuthenticationHelper::username() const
{
    return m_username;
}

const std::optional<header::DigestAuthorization>& AuthenticationHelper::authzHeader() const
{
    return m_authzHeader;
}

bool AuthenticationHelper::userLocked() const
{
    if (!m_username.empty() &&
        m_userLocker &&
        m_userLocker->isLocked(m_userLockKey))
    {
        return true;
    }

    return false;
}

void AuthenticationHelper::reportSuccess(
    std::optional<nx::utils::stree::ResourceContainer> authProperties)
{
    nx::utils::swapAndCall(
        m_handler,
        prepareSuccessResponse(std::move(authProperties)));
}

void AuthenticationHelper::reportFailure(
    api::ResultCode resultCode,
    std::optional<nx::network::http::header::WWWAuthenticate> wwwAuthenticate)
{
    nx::utils::swapAndCall(
        m_handler,
        prepareUnauthorizedResponse(resultCode, std::move(wwwAuthenticate)));
}

void AuthenticationHelper::queryStaticAuthenticationRulesTree(
    const StreeManager& stree)
{
    nx::utils::stree::ResourceContainer inputRes;
    if (!m_username.empty())
        inputRes.put(attr::userName, nx::String(m_username.c_str()));
    SocketResourceReader socketResources(*m_connection.socket());
    HttpRequestResourceReader httpRequestResources(m_request);
    const auto authSearchInputData = nx::utils::stree::MultiSourceResourceReader(
        socketResources,
        httpRequestResources,
        inputRes,
        m_authTraversalResult);

    stree.search(
        StreeOperation::authentication,
        authSearchInputData,
        &m_authTraversalResult);
}

bool AuthenticationHelper::authenticatedByStaticRules() const
{
    // TODO: #ak Merge authRestrictionList & stree.

    const auto allowedAuthMethods = m_authRestrictionList.getAllowedAuthMethods(m_request);
    if (allowedAuthMethods & AuthMethod::noAuth)
        return true;

    if (auto authenticated = m_authTraversalResult.get(attr::authenticated))
    {
        if (authenticated.get().toBool())
            return true;
    }

    return false;
}

const std::function<bool(const nx::Buffer& /*ha1*/)>&
    AuthenticationHelper::validateHa1Func() const
{
    return m_validateHa1Func;
}

bool AuthenticationHelper::requestContainsValidDigest() const
{
    if (!m_authzHeader ||
        (m_authzHeader->authScheme != header::AuthScheme::digest) ||
        m_username.empty() ||
        !validateNonce(m_authzHeader->digest->params["nonce"]))
    {
        return false;
    }

    return true;
}

api::ResultCode AuthenticationHelper::authenticateRequestDigest(
    const std::vector<AbstractAuthenticationDataProvider*>& authDataProviders,
    nx::utils::stree::ResourceContainer* const authProperties)
{
    using AuthResult = nx::network::server::UserLocker::AuthResult;

    api::ResultCode authResultCode = api::ResultCode::notAuthorized;
    if (streeQueryFoundPasswordMatchesRequestDigest())
    {
        authResultCode = api::ResultCode::ok;
    }
    else
    {
        authResultCode = authenticateInDataManagers(
            authDataProviders,
            authProperties);
    }

    if (authResultCode == api::ResultCode::ok)
        updateUserLockoutState(AuthResult::success);
    else if (authResultCode == api::ResultCode::notAuthorized) //< I.e., wrong digest.
        updateUserLockoutState(AuthResult::failure);

    return authResultCode;
}

void AuthenticationHelper::loadAuthorizationHeader()
{
    const auto authHeaderIter = m_request.headers.find(header::Authorization::NAME);
    if (authHeaderIter != m_request.headers.end())
    {
        m_authzHeader.emplace(header::DigestAuthorization());
        if (!m_authzHeader->parse(authHeaderIter->second))
            m_authzHeader = std::nullopt;
    }
}

nx::network::http::server::AuthenticationResult
    AuthenticationHelper::prepareSuccessResponse(
        std::optional<nx::utils::stree::ResourceContainer> authProperties)
{
    nx::network::http::server::AuthenticationResult authResponse;
    authResponse.isSucceeded = true;
    if (authProperties)
        authResponse.authInfo = std::move(*authProperties);
    return authResponse;
}

nx::network::http::server::AuthenticationResult
    AuthenticationHelper::prepareUnauthorizedResponse(
        api::ResultCode authResult,
        std::optional<nx::network::http::header::WWWAuthenticate> wwwAuthenticate)
{
    nx::network::http::server::AuthenticationResult authResponse;
    prepareUnauthorizedResponse(authResult, &authResponse);
    if (wwwAuthenticate)
        authResponse.wwwAuthenticate = wwwAuthenticate;
    return authResponse;
}

void AuthenticationHelper::prepareUnauthorizedResponse(
    api::ResultCode authResult,
    nx::network::http::server::AuthenticationResult* authResponse)
{
    authResponse->responseHeaders.emplace(
        Qn::API_RESULT_CODE_HEADER_NAME,
        QnLexical::serialized(authResult).toLatin1());

    nx::network::http::FusionRequestResult result(
        nx::network::http::FusionRequestErrorClass::unauthorized,
        QnLexical::serialized(authResult),
        static_cast<int>(authResult),
        "unauthorized");

    authResponse->msgBody = std::make_unique<nx::network::http::BufferSource>(
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
        QJson::serialized(result));
}

bool AuthenticationHelper::validateNonce(
    const nx::network::http::StringType& nonce) const
{
    // TODO: #ak introduce more strong nonce validation.
    // Currently, forbidding authentication with nonce, generated by /auth/get_nonce request.
    return nonce.size() < 31;
}


bool AuthenticationHelper::streeQueryFoundPasswordMatchesRequestDigest()
{
    if (auto foundHa1 = m_authTraversalResult.get(attr::ha1))
    {
        if (m_validateHa1Func(foundHa1.get().toString().toLatin1()))
            return true;
    }

    if (auto password = m_authTraversalResult.get(attr::userPassword))
    {
        if (m_validateHa1Func(calcHa1(
                m_username.c_str(),
                nx::network::AppInfo::realm().toUtf8(),
                password.get().toString())))
        {
            return true;
        }
    }

    return false;
}

api::ResultCode AuthenticationHelper::authenticateInDataManagers(
    const std::vector<AbstractAuthenticationDataProvider*>& authDataProviders,
    nx::utils::stree::ResourceContainer* const authProperties)
{
    // TODO: #ak AuthenticationManager has to become async some time...

    std::vector<api::ResultCode> authDataProvidersResults;
    authDataProvidersResults.reserve(authDataProviders.size());
    for (AbstractAuthenticationDataProvider* authDataProvider: authDataProviders)
    {
        nx::utils::promise<api::ResultCode> authPromise;
        auto authFuture = authPromise.get_future();
        authDataProvider->authenticateByName(
            m_username.c_str(),
            m_validateHa1Func,
            authProperties,
            [&authPromise](api::ResultCode authResult)
            {
                authPromise.set_value(authResult);
            });
        authFuture.wait();
        const auto result = authFuture.get();
        if (result == api::ResultCode::ok ||
            result == api::ResultCode::credentialsRemovedPermanently)
        {
            return result;
        }
        authDataProvidersResults.push_back(result);
    }

    if (std::all_of(
            authDataProvidersResults.begin(),
            authDataProvidersResults.end(),
            [](auto result) { return result == api::ResultCode::badUsername; }))
    {
        return api::ResultCode::badUsername;
    }

    return api::ResultCode::notAuthorized;
}

void AuthenticationHelper::updateUserLockoutState(
    network::server::UserLocker::AuthResult authResult)
{
    if (m_userLocker)
        m_userLocker->updateLockoutState(m_userLockKey, authResult);
}

} // namespace detail

//-------------------------------------------------------------------------------------------------

AuthenticationManager::AuthenticationManager(
    const conf::Settings& settings,
    std::vector<AbstractAuthenticationDataProvider*> authDataProviders,
    const nx::network::http::AuthMethodRestrictionList& authRestrictionList,
    const StreeManager& stree)
:
    m_authRestrictionList(authRestrictionList),
    m_stree(stree),
    m_authDataProviders(std::move(authDataProviders))
{
    if (auto userLockerSettings = settings.loginLockout())
        m_userLocker = std::make_unique<network::server::UserLockerPool>(*userLockerSettings);
}

void AuthenticationManager::authenticate(
    const nx::network::http::HttpServerConnection& connection,
    const nx::network::http::Request& request,
    nx::network::http::server::AuthenticationCompletionHandler handler)
{
    detail::AuthenticationHelper authenticatorHelper(
        m_authRestrictionList,
        m_userLocker.get(),
        connection,
        request,
        std::move(handler));

    if (authenticatorHelper.userLocked())
        return authenticatorHelper.reportFailure(api::ResultCode::accountBlocked);

    authenticatorHelper.queryStaticAuthenticationRulesTree(m_stree);
    if (authenticatorHelper.authenticatedByStaticRules())
        return authenticatorHelper.reportSuccess();

    if (!authenticatorHelper.requestContainsValidDigest())
    {
        return authenticatorHelper.reportFailure(
            api::ResultCode::notAuthorized,
            prepareWwwAuthenticateHeader());
    }

    nx::utils::stree::ResourceContainer authProperties;
    const auto authResultCode = authenticatorHelper.authenticateRequestDigest(
        m_authDataProviders,
        &authProperties);

    if (authResultCode == api::ResultCode::ok)
        return authenticatorHelper.reportSuccess(std::move(authProperties));
    else
        return authenticatorHelper.reportFailure(authResultCode);
}

nx::String AuthenticationManager::realm()
{
    return nx::network::AppInfo::realm().toUtf8();
}

nx::network::http::header::WWWAuthenticate
    AuthenticationManager::prepareWwwAuthenticateHeader()
{
    nx::network::http::header::WWWAuthenticate wwwAuthenticate;

    wwwAuthenticate.authScheme = header::AuthScheme::digest;
    wwwAuthenticate.params.insert("nonce", generateNonce());
    wwwAuthenticate.params.insert("realm", realm());
    wwwAuthenticate.params.insert("algorithm", "MD5");

    return wwwAuthenticate;
}

nx::Buffer AuthenticationManager::generateNonce()
{
    const auto nonce =
        nx::utils::random::number<nx::utils::random::CryptographicRandomDevice, uint64_t>(
            nx::utils::random::CryptographicRandomDevice::instance())
        | nx::utils::timeSinceEpoch().count();
    return nx::Buffer::number((qulonglong)nonce);
}

} // namespace cdb
} // namespace nx
