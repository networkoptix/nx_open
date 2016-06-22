/**********************************************************
* 6 may 2015
* a.kolesnikov
***********************************************************/

#include "authentication_manager.h"

#include <future>
#include <limits>

#include <boost/optional.hpp>

#include <cloud_db_client/src/data/types.h>

#include <nx/network/auth_restriction_list.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/server/fusion_request_result.h>

#include <http/custom_headers.h>
#include <utils/common/app_info.h>
#include <utils/common/guard.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/lexical.h>

#include "abstract_authentication_data_provider.h"
#include "stree/cdb_ns.h"
#include "managers/account_manager.h"
#include "managers/temporary_account_password_manager.h"
#include "managers/system_manager.h"
#include "stree/http_request_attr_reader.h"
#include "stree/socket_attr_reader.h"
#include "stree/stree_manager.h"


namespace nx {
namespace cdb {

using namespace nx_http;

AuthenticationManager::AuthenticationManager(
    std::vector<AbstractAuthenticationDataProvider*> authDataProviders,
    const QnAuthMethodRestrictionList& authRestrictionList,
    const StreeManager& stree)
:
    m_authRestrictionList(authRestrictionList),
    m_stree(stree),
    m_dist(0, std::numeric_limits<size_t>::max()),
    m_authDataProviders(std::move(authDataProviders))
{
}

void AuthenticationManager::authenticate(
    const nx_http::HttpServerConnection& connection,
    const nx_http::Request& request,
    nx::utils::MoveOnlyFunc<void(
        bool authenticationResult,
        stree::ResourceContainer authInfo,
        boost::optional<nx_http::header::WWWAuthenticate> wwwAuthenticate,
        nx_http::HttpHeaders responseHeaders,
        std::unique_ptr<nx_http::AbstractMsgBodySource> msgBody)> completionHandler)
{
    boost::optional<nx_http::header::WWWAuthenticate> wwwAuthenticate;
    stree::ResourceContainer authProperties;
    nx_http::HttpHeaders responseHeaders;
    std::unique_ptr<nx_http::AbstractMsgBodySource> msgBody;

    api::ResultCode authResult = api::ResultCode::notAuthorized;
    auto guard = makeScopedGuard(
        [&authResult, &wwwAuthenticate, &authProperties, 
        &responseHeaders, &msgBody, &completionHandler]()
        {
            if (authResult != api::ResultCode::ok)
            {
                nx_http::FusionRequestResult result(
                    nx_http::FusionRequestErrorClass::unauthorized,
                    QnLexical::serialized(authResult),
                    static_cast<int>(authResult),
                    "unauthorized");
                responseHeaders.emplace(
                    Qn::API_RESULT_CODE_HEADER_NAME,
                    QnLexical::serialized(authResult).toLatin1());
                msgBody = std::make_unique<nx_http::BufferSource>(
                    Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
                    QJson::serialized(result));
            }

            completionHandler(
                authResult == api::ResultCode::ok,
                std::move(authProperties),
                std::move(wwwAuthenticate),
                std::move(responseHeaders),
                std::move(msgBody));
        });

    //TODO #ak use QnAuthHelper class to enable all that authentication types

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

    //checking header
    boost::optional<header::DigestAuthorization> authzHeader;
    if (authHeaderIter != request.headers.end())
    {
        authzHeader.emplace(header::DigestAuthorization());
        if (!authzHeader->parse(authHeaderIter->second))
            authzHeader.reset();
    }

    //performing stree search
    stree::ResourceContainer authTraversalResult;
    stree::ResourceContainer inputRes;
    if (authzHeader && !authzHeader->userid().isEmpty())
        inputRes.put(attr::userName, authzHeader->userid());
    SocketResourceReader socketResources(*connection.socket());
    HttpRequestResourceReader httpRequestResources(request);
    const auto authSearchInputData = stree::MultiSourceResourceReader(
        socketResources,
        httpRequestResources,
        inputRes);
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

    //analyzing authSearchResult for password or ha1 pesence
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
    return QnAppInfo::realm().toUtf8();
}

bool AuthenticationManager::validateNonce(const nx_http::StringType& nonce)
{
    //TODO #ak introduce more strong nonce validation
        //Currently, forbidding authentication with nonce, generated by /auth/get_nonce request
    return nonce.size() < 31;
}

api::ResultCode AuthenticationManager::authenticateInDataManagers(
    const nx_http::StringType& username,
    std::function<bool(const nx::Buffer&)> validateHa1Func,
    const stree::AbstractResourceReader& authSearchInputData,
    stree::ResourceContainer* const authProperties)
{
    //TODO #ak AuthenticationManager has to become async some time...

    for (AbstractAuthenticationDataProvider* authDataProvider: m_authDataProviders)
    {
        std::promise<api::ResultCode> authPromise;
        auto authFuture = authPromise.get_future();
        authDataProvider->authenticateByName(
            username,
            validateHa1Func,
            authSearchInputData,
            authProperties,
            [&authPromise](api::ResultCode authResult) {
                authPromise.set_value(authResult);
            });
        authFuture.wait();
        const auto result = authFuture.get();
        if (result != api::ResultCode::notAuthorized)
            return result;  //"ok" or "credentialsRemovedPermanently"
    }

    return api::ResultCode::notAuthorized;
}

void AuthenticationManager::addWWWAuthenticateHeader(
    boost::optional<nx_http::header::WWWAuthenticate>* const wwwAuthenticate)
{
    //adding WWW-Authenticate header
    *wwwAuthenticate = header::WWWAuthenticate();
    wwwAuthenticate->get().authScheme = header::AuthScheme::digest;
    wwwAuthenticate->get().params.insert("nonce", generateNonce());
    wwwAuthenticate->get().params.insert("realm", realm());
    wwwAuthenticate->get().params.insert("algorithm", "MD5");
}

nx::Buffer AuthenticationManager::generateNonce()
{
    const size_t nonce = m_dist(m_rd) | ::time(NULL);
    return nx::Buffer::number((qulonglong)nonce);
}

}   //cdb
}   //nx
