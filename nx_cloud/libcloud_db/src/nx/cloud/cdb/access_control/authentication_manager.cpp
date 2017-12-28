#include "authentication_manager.h"

#include <future>
#include <limits>

#include <boost/optional.hpp>

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

AuthenticationManager::AuthenticationManager(
    std::vector<AbstractAuthenticationDataProvider*> authDataProviders,
    const nx::network::http::AuthMethodRestrictionList& authRestrictionList,
    const StreeManager& stree)
:
    m_authRestrictionList(authRestrictionList),
    m_stree(stree),
    m_authDataProviders(std::move(authDataProviders))
{
}

void AuthenticationManager::authenticate(
    const nx::network::http::HttpServerConnection& connection,
    const nx::network::http::Request& request,
    nx::network::http::server::AuthenticationCompletionHandler completionHandler)
{
    boost::optional<nx::network::http::header::WWWAuthenticate> wwwAuthenticate;
    nx::utils::stree::ResourceContainer authProperties;
    nx::network::http::HttpHeaders responseHeaders;
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody;

    api::ResultCode authResult = api::ResultCode::notAuthorized;
    auto guard = makeScopeGuard(
        [&authResult, &wwwAuthenticate, &authProperties,
        &responseHeaders, &msgBody, &completionHandler]()
        {
            if (authResult != api::ResultCode::ok)
            {
                nx::network::http::FusionRequestResult result(
                    nx::network::http::FusionRequestErrorClass::unauthorized,
                    QnLexical::serialized(authResult),
                    static_cast<int>(authResult),
                    "unauthorized");
                responseHeaders.emplace(
                    Qn::API_RESULT_CODE_HEADER_NAME,
                    QnLexical::serialized(authResult).toLatin1());
                msgBody = std::make_unique<nx::network::http::BufferSource>(
                    Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
                    QJson::serialized(result));
            }

            completionHandler(
                nx::network::http::server::AuthenticationResult(
                    authResult == api::ResultCode::ok,
                    std::move(authProperties),
                    std::move(wwwAuthenticate),
                    std::move(responseHeaders),
                    std::move(msgBody)));
        });

    // TODO: #ak Use QnAuthHelper class to enable all that authentication types.

    const auto allowedAuthMethods = m_authRestrictionList.getAllowedAuthMethods(request);
    if (allowedAuthMethods & AuthMethod::noAuth)
    {
        authResult = api::ResultCode::ok;
        return;
    }
    if (!(allowedAuthMethods & AuthMethod::httpDigest))
    {
        authResult = api::ResultCode::notAuthorized;
        return;
    }

    const auto authHeaderIter = request.headers.find(header::Authorization::NAME);

    // Checking header.
    boost::optional<header::DigestAuthorization> authzHeader;
    if (authHeaderIter != request.headers.end())
    {
        authzHeader.emplace(header::DigestAuthorization());
        if (!authzHeader->parse(authHeaderIter->second))
            authzHeader.reset();
    }

    // Performing stree search.
    nx::utils::stree::ResourceContainer authTraversalResult;
    nx::utils::stree::ResourceContainer inputRes;
    if (authzHeader && !authzHeader->userid().isEmpty())
        inputRes.put(attr::userName, authzHeader->userid());
    SocketResourceReader socketResources(*connection.socket());
    HttpRequestResourceReader httpRequestResources(request);
    const auto authSearchInputData = nx::utils::stree::MultiSourceResourceReader(
        socketResources,
        httpRequestResources,
        inputRes,
        authTraversalResult);
    m_stree.search(
        StreeOperation::authentication,
        authSearchInputData,
        &authTraversalResult);
    if (auto authenticated = authTraversalResult.get(attr::authenticated))
    {
        if (authenticated.get().toBool())
        {
            authResult = api::ResultCode::ok;
            return;
        }
    }

    if (!authzHeader ||
        (authzHeader->authScheme != header::AuthScheme::digest) ||
        (authzHeader->userid().isEmpty()))
    {
        addWWWAuthenticateHeader(&wwwAuthenticate);
        authResult = api::ResultCode::notAuthorized;
        return;
    }

    if (!validateNonce(authzHeader->digest->params["nonce"]))
    {
        addWWWAuthenticateHeader(&wwwAuthenticate);
        authResult = api::ResultCode::notAuthorized;
        return;
    }

    const auto userID = authzHeader->userid();
    std::function<bool(const nx::Buffer&)> validateHa1Func = std::bind(
        &validateAuthorization,
        request.requestLine.method,
        userID,
        boost::none,
        std::placeholders::_1,
        std::move(authzHeader.get()));

    // Analyzing authSearchResult for password or ha1 pesence.
    if (auto foundHa1 = authTraversalResult.get(attr::ha1))
    {
        if (validateHa1Func(foundHa1.get().toString().toLatin1()))
        {
            authResult = api::ResultCode::ok;
            return;
        }
    }
    if (auto password = authTraversalResult.get(attr::userPassword))
    {
        if (validateHa1Func(calcHa1(
                userID,
                realm(),
                password.get().toString())))
        {
            authResult = api::ResultCode::ok;
            return;
        }
    }

    authResult = authenticateInDataManagers(
        userID,
        std::move(validateHa1Func),
        authSearchInputData,
        &authProperties);
}

nx::String AuthenticationManager::realm()
{
    return nx::network::AppInfo::realm().toUtf8();
}

bool AuthenticationManager::validateNonce(const nx::network::http::StringType& nonce)
{
    // TODO: #ak introduce more strong nonce validation.
    // Currently, forbidding authentication with nonce, generated by /auth/get_nonce request.
    return nonce.size() < 31;
}

api::ResultCode AuthenticationManager::authenticateInDataManagers(
    const nx::network::http::StringType& username,
    std::function<bool(const nx::Buffer&)> validateHa1Func,
    const nx::utils::stree::AbstractResourceReader& authSearchInputData,
    nx::utils::stree::ResourceContainer* const authProperties)
{
    // TODO: #ak AuthenticationManager has to become async some time...

    for (AbstractAuthenticationDataProvider* authDataProvider: m_authDataProviders)
    {
        nx::utils::promise<api::ResultCode> authPromise;
        auto authFuture = authPromise.get_future();
        authDataProvider->authenticateByName(
            username,
            validateHa1Func,
            authSearchInputData,
            authProperties,
            [&authPromise](api::ResultCode authResult)
            {
                authPromise.set_value(authResult);
            });
        authFuture.wait();
        const auto result = authFuture.get();
        if (result != api::ResultCode::notAuthorized)
            return result;  //< "ok" or "credentialsRemovedPermanently".
    }

    return api::ResultCode::notAuthorized;
}

void AuthenticationManager::addWWWAuthenticateHeader(
    boost::optional<nx::network::http::header::WWWAuthenticate>* const wwwAuthenticate)
{
    // Adding WWW-Authenticate header.
    *wwwAuthenticate = header::WWWAuthenticate();
    wwwAuthenticate->get().authScheme = header::AuthScheme::digest;
    wwwAuthenticate->get().params.insert("nonce", generateNonce());
    wwwAuthenticate->get().params.insert("realm", realm());
    wwwAuthenticate->get().params.insert("algorithm", "MD5");
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
