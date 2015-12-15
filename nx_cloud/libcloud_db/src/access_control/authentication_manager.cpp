/**********************************************************
* 6 may 2015
* a.kolesnikov
***********************************************************/

#include "authentication_manager.h"

#include <future>
#include <limits>

#include <boost/optional.hpp>

#include <utils/common/app_info.h>
#include <utils/common/guard.h>
#include <utils/serialization/json.h>
#include <nx/network/auth_restriction_list.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/server/fusion_request_result.h>

#include "abstract_authentication_data_provider.h"
#include "stree/cdb_ns.h"
#include "managers/account_manager.h"
#include "managers/temporary_account_password_manager.h"
#include "managers/system_manager.h"
#include "stree/http_request_attr_reader.h"
#include "stree/socket_attr_reader.h"
#include "stree/stree_manager.h"
#include "version.h"


namespace nx {
namespace cdb {

using namespace nx_http;

AuthenticationManager::AuthenticationManager(
    AccountManager* const accountManager,
    SystemManager* const systemManager,
    TemporaryAccountPasswordManager* const temporaryAccountPasswordManager,
    const QnAuthMethodRestrictionList& authRestrictionList,
    const StreeManager& stree)
:
    m_authRestrictionList(authRestrictionList),
    m_stree(stree),
    m_dist(0, std::numeric_limits<size_t>::max())
{
    m_authDataProviders.push_back(accountManager);
    m_authDataProviders.push_back(systemManager);
    m_authDataProviders.push_back(temporaryAccountPasswordManager);
}

bool AuthenticationManager::authenticate(
    const nx_http::HttpServerConnection& connection,
    const nx_http::Request& request,
    boost::optional<nx_http::header::WWWAuthenticate>* const wwwAuthenticate,
    stree::AbstractResourceWriter* authProperties,
    std::unique_ptr<nx_http::AbstractMsgBodySource>* const msgBody)
{
    bool authResult = false;
    auto guard = makeScopedGuard([&msgBody, &authResult](){
        if (authResult)
            return;
        nx_http::FusionRequestResult result;
        result.resultCode = nx_http::FusionRequestErrorClass::unauthorized;
        result.errorText = "unauthorized";
        *msgBody = std::make_unique<nx_http::BufferSource>(
            Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
            QJson::serialized(result));
    });

    //TODO #ak use QnAuthHelper class to support all that authentication types

    const auto allowedAuthMethods = m_authRestrictionList.getAllowedAuthMethods(request);
    if (allowedAuthMethods & AuthMethod::noAuth)
        return (authResult = true);
    if (!(allowedAuthMethods & AuthMethod::httpDigest))
        return (authResult = false);

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
            return (authResult = true);
    }

    if (!authzHeader ||
        (authzHeader->authScheme != header::AuthScheme::digest) ||
        (authzHeader->userid().isEmpty()))
    {
        addWWWAuthenticateHeader(wwwAuthenticate);
        return (authResult = false);
    }

    if (!validateNonce(authzHeader->digest->params["nonce"]))
    {
        addWWWAuthenticateHeader(wwwAuthenticate);
        return (authResult = false);
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
            return (authResult = true);
    }
    if (auto password = authTraversalResult.get(attr::userPassword))
    {
        if (validateHa1Func(calcHa1(
                userID,
                realm(),
                password.get().toString())))
        {
            return (authResult = true);
        }
    }

    if (!authenticateInDataManagers(
            userID,
            std::move(validateHa1Func),
            authSearchInputData,
            authProperties))
    {
        return (authResult = false);
    }

    return (authResult = true);
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

bool AuthenticationManager::authenticateInDataManagers(
    const nx_http::StringType& username,
    std::function<bool(const nx::Buffer&)> validateHa1Func,
    const stree::AbstractResourceReader& authSearchInputData,
    stree::AbstractResourceWriter* const authProperties)
{
    //TODO #ak AuthenticationManager has to become async sometimes...

    for (AbstractAuthenticationDataProvider* authDataProvider: m_authDataProviders)
    {
        std::promise<bool> authPromise;
        auto authFuture = authPromise.get_future();
        authDataProvider->authenticateByName(
            username,
            validateHa1Func,
            authSearchInputData,
            authProperties,
            [&authPromise](bool authResult) { authPromise.set_value(authResult); });
        authFuture.wait();
        if (authFuture.get())
            return true;
    }

    return false;
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
