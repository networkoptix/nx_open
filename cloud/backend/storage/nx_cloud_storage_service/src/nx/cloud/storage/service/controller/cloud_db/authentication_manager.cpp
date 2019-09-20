#include "authentication_manager.h"

#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/buffer_source.h>
#include <nx/cloud/db/api/cdb_client.h>
#include <nx/network/http/auth_tools.h>

#include "nx/cloud/storage/service/settings.h"
#include "resource.h"

namespace nx::cloud::storage::service::controller::cloud_db{

using namespace nx::cloud::db::api;
using namespace nx::network::http;

namespace {

static QString toString(const UserAuthorization& userAuth)
{
	return lm("{ requestMethod: %1, requestAuthorization: %2 }")
		.args(userAuth.requestMethod, userAuth.requestAuthorization);
}

static QString toString(const CredentialsDescriptor& credentials)
{
	return lm("{ status: %1, objectId: %2, objectType: %3 }").args(
		credentials.status,
		credentials.objectId,
		toString(credentials.objectType));
}

} // namespace

AuthenticationForwarder::AuthenticationForwarder(const conf::CloudDb& settings):
    m_settings(settings)
{
}

void AuthenticationForwarder::authenticate(
    const HttpServerConnection& /*connection*/,
    const Request& request,
    server::AuthenticationCompletionHandler completionHandler)
{
    auto authorizationIt = request.headers.find(header::Authorization::NAME);
    if (authorizationIt == request.headers.end())
    {
        auto failedAuthentication =
            prepareFailedAuthenticationResult(
                ResultCode::notAuthorized,
                "missing Authorization header");
        failedAuthentication.wwwAuthenticate = generateWwwAuthenticateHeader(generateNonce());
        return completionHandler(std::move(failedAuthentication));
    }

    UserAuthorization userAuth;
    userAuth.requestMethod = request.requestLine.method.toStdString();
    userAuth.requestAuthorization = authorizationIt->second.toStdString();

    authenticateWithCloudDb(std::move(userAuth), std::move(completionHandler));
}

void AuthenticationForwarder::authenticateWithCloudDb(
    UserAuthorization userAuth,
    server::AuthenticationCompletionHandler completionHandler)
{
    using namespace std::placeholders;

    auto* cdbClient = createAuthenticationRequest(std::move(completionHandler));
    cdbClient->authProvider()->resolveUserCredentials(
        userAuth,
        [this, cdbClient, userAuth](
            auto cdbResult,
            auto credentials) mutable
        {
            auto request = takeRequestContext(cdbClient);

            if (cdbResult != ResultCode::ok)
            {
                return request.handler(
                    prepareFailedAuthenticationResult(cdbResult, "resolveUserCredentials failed"));
            }

			NX_VERBOSE(this, "CloudDb authentication request: %1, reporting result: %2",
				toString(userAuth),
				toString(credentials));

            if (credentials.objectType == ObjectType::none || credentials.objectId.empty())
            {
                return request.handler(
                    prepareFailedAuthenticationResult(
                        ResultCode::unknownError,
                        "received unexpected ObjectType in credentials"));
            }

            server::SuccessfulAuthenticationResult success;
			// There are some api requests that only account owners are allowed to make,
			// e.g. "addStorage". if system is making the request, it will fail because authInfo
			// is missing accountEmail.
			if (credentials.objectType == ObjectType::account)
				success.authInfo.put(Resource::accountEmail, credentials.objectId.c_str());
            success.authInfo.put(Resource::httpMethod, userAuth.requestMethod.c_str());
            success.authInfo.put(Resource::authorization, userAuth.requestAuthorization.c_str());

            request.handler(std::move(success));
        });
}


CdbClient* AuthenticationForwarder::createAuthenticationRequest(
    server::AuthenticationCompletionHandler completionHandler)
{
    RequestContext request;
    request.cdbClient = std::make_unique<CdbClient>();
    request.cdbClient->setCloudUrl(m_settings.url.toStdString());
    request.handler = std::move(completionHandler);

    auto cdbClientPtr = request.cdbClient.get();

    QnMutexLocker lock(&m_mutex);
    m_cdbContexts.emplace(cdbClientPtr, std::move(request));

    return cdbClientPtr;
}

AuthenticationForwarder::RequestContext
    AuthenticationForwarder::takeRequestContext(
        CdbClient* cdbClient)
{
    QnMutexLocker lock(&m_mutex);
    auto it = m_cdbContexts.find(cdbClient);
    NX_ASSERT(it != m_cdbContexts.end());
    auto request = std::move(it->second);
    m_cdbContexts.erase(it);
    return request;
}

server::AuthenticationResult AuthenticationForwarder::prepareFailedAuthenticationResult(
    ResultCode resultCode,
    QByteArray reason)
{
    auto message = lm("Cloud DB authentication failed with result code: %1, %2")
        .args(toString(resultCode), reason).toUtf8();

    NX_VERBOSE(this, message);

    server::AuthenticationResult failedAuthentication;
    failedAuthentication.msgBody =
        std::make_unique<BufferSource>("text/plain", std::move(message));

    return failedAuthentication;
}

//-------------------------------------------------------------------------------------------------
// AuthenticationManagerFactory

AuthenticationManagerFactory::AuthenticationManagerFactory():
    base_type(std::bind(
        &AuthenticationManagerFactory::defaultFactoryFunction,
        this,
        std::placeholders::_1))
{
}

AuthenticationManagerFactory& AuthenticationManagerFactory::instance()
{
    static AuthenticationManagerFactory factory;
    return factory;
}

std::unique_ptr<server::AbstractAuthenticationManager>
    AuthenticationManagerFactory::defaultFactoryFunction(
        const conf::Settings& settings)
{
    return std::make_unique<AuthenticationForwarder>(settings.cloudDb());
}

} // namespace nx::cloud::storage::service::view::http
