#include "cloud_db_authentication_manager.h"

#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/buffer_source.h>
#include <nx/cloud/db/api/cdb_client.h>
#include <nx/network/http/auth_tools.h>

#include "nx/cloud/storage/service/settings.h"
#include "cloud_db_resource.h"

namespace nx::cloud::storage::service::view::http {

using namespace nx::cloud::db::api;
using namespace nx::network::http;

CloudDbAuthenticationForwarder::CloudDbAuthenticationForwarder(const conf::CloudDb& settings):
    m_settings(settings)
{
}

void CloudDbAuthenticationForwarder::authenticate(
    const HttpServerConnection& connection,
    const Request& request,
    server::AuthenticationCompletionHandler completionHandler)
{
    auto authorizationIt = request.headers.find(header::Authorization::NAME);
    if (authorizationIt == request.headers.end())
    {
        auto failedAuthentication =
            failedAuthenticationResult(
                ResultCode::badRequest,
                "missing Authorization header");
        failedAuthentication.wwwAuthenticate = generateWwwAuthenticateHeader(generateNonce());
        return completionHandler(std::move(failedAuthentication));
    }

    UserAuthorization userAuth;
    userAuth.requestMethod = request.requestLine.method.toStdString();
    userAuth.requestAuthorization = authorizationIt->second.toStdString();

    authenticateWithCloudDb(std::move(userAuth), std::move(completionHandler));
}

void CloudDbAuthenticationForwarder::authenticateWithCloudDb(
    UserAuthorization userAuth,
    server::AuthenticationCompletionHandler completionHandler)
{
    using namespace std::placeholders;

    auto* cdbClient = createAuthenticationRequest(std::move(completionHandler));
    cdbClient->authProvider()->resolveUserDigest(
        userAuth,
        [this, cdbClient](
            auto cdbResult,
            auto credentials) mutable
        {
            auto request = authenticationComplete(cdbClient);

            if (cdbResult != ResultCode::ok)
            {
                return request.handler(
                    failedAuthenticationResult(cdbResult, "resolveUserDigestFailed"));
            }

            if (credentials.objectType != ObjectType::account || credentials.objectId.empty())
            {
                return request.handler(
                    failedAuthenticationResult(
                        ResultCode::unknownError,
                        "received unexpected ObjectType in credentials"));
            }

            server::SuccessfulAuthenticationResult success;
            success.authInfo.put(CloudDbResource::accountOwner, credentials.objectId.c_str());
            request.handler(std::move(success));
        });
}


CdbClient* CloudDbAuthenticationForwarder::createAuthenticationRequest(
    server::AuthenticationCompletionHandler completionHandler)
{
    AuthenticationRequest request;
    request.cdbClient = std::make_unique<CdbClient>();
    request.cdbClient->setCloudUrl(m_settings.url.toStdString());
    request.handler = std::move(completionHandler);

    auto cdbClientPtr = request.cdbClient.get();

    QnMutexLocker lock(&m_mutex);
    m_cdbContexts.emplace(cdbClientPtr, std::move(request));

    return cdbClientPtr;
}

CloudDbAuthenticationForwarder::AuthenticationRequest
    CloudDbAuthenticationForwarder::authenticationComplete(
        CdbClient* cdbClient)
{
    QnMutexLocker lock(&m_mutex);
    auto it = m_cdbContexts.find(cdbClient);
    NX_ASSERT(it != m_cdbContexts.end());
    auto request = std::move(it->second);
    m_cdbContexts.erase(it);
    return request;
}

server::AuthenticationResult CloudDbAuthenticationForwarder::failedAuthenticationResult(
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
// CloudDbAuthenticationFactory

CloudDbAuthenticationFactory::CloudDbAuthenticationFactory():
    base_type(std::bind(
        &CloudDbAuthenticationFactory::defaultFactoryFunction,
        this,
        std::placeholders::_1))
{
}

CloudDbAuthenticationFactory& CloudDbAuthenticationFactory::instance()
{
    static CloudDbAuthenticationFactory factory;
    return factory;
}

std::unique_ptr<server::AbstractAuthenticationManager>
    CloudDbAuthenticationFactory::defaultFactoryFunction(
        const conf::Settings& settings)
{
    return std::make_unique<CloudDbAuthenticationForwarder>(settings.cloudDb());
}

} // namespace nx::cloud::storage::service::view::http
