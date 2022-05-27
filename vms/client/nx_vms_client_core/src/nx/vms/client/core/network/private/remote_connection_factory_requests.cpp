// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_factory_requests.h"

#include <chrono>
#include <memory>

#include <QtCore/QPointer>

#include <core/resource/resource_property_key.h>
#include <nx/fusion/serialization_format.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/nx_network_ini.h>
#include <nx/network/rest/result.h>
#include <nx/network/ssl/certificate.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/enum_string_conversion.h>
#include <nx/reflect/json.h>
#include <nx/utils/log/log.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/utils/serialization/qt_containers_reflect_json.h>
#include <nx/vms/api/data/server_model.h>
#include <nx/vms/auth/auth_result.h>
#include <nx/vms/client/core/utils/cloud_session_token_updater.h>
#include <nx_ec/ec_api_common.h>

#include "../network_manager.h"
#include "../certificate_verifier.h"

using namespace ec2;
using namespace nx::network::http;
using namespace std::chrono;

namespace nx::vms::client::core {

namespace {

using ExternalErrorHandler =
    std::function<std::optional<RemoteConnectionErrorCode>(const NetworkManager::Response&)>;

using ExternalErrorMap = std::map<int, RemoteConnectionErrorCode>;

ExternalErrorHandler expectedErrorCodes(ExternalErrorMap errorCodes)
{
    return
        [errorCodes](const NetworkManager::Response& response)
            -> std::optional<RemoteConnectionErrorCode>
        {
            auto iter = errorCodes.find(response.statusLine.statusCode);
            if (iter == errorCodes.cend())
                return std::nullopt;

            return iter->second;
        };
}

std::optional<RemoteConnectionErrorCode> unauthorizedErrorDetails(const HttpHeaders& headers)
{
    using AuthResult = nx::vms::common::AuthResult;

    AuthResult authResult;
    const auto authResultStr = getHeaderValue(headers, Qn::AUTH_RESULT_HEADER_NAME);

    if (authResultStr.empty() || !nx::reflect::fromString<AuthResult>(authResultStr, &authResult))
        return std::nullopt;

    if (authResult == AuthResult::Auth_LDAPConnectError)
        return RemoteConnectionErrorCode::ldapInitializationInProgress;

    if (authResult == AuthResult::Auth_CloudConnectError)
        return RemoteConnectionErrorCode::cloudUnavailableOnServer;

    if (authResult == AuthResult::Auth_DisabledUser)
        return RemoteConnectionErrorCode::userIsDisabled;

    if (authResult == AuthResult::Auth_LockedOut)
        return RemoteConnectionErrorCode::userIsLockedOut;

    return std::nullopt;
}

RemoteConnectionErrorCode toConnectionErrorCode(int statusCode, const HttpHeaders& headers)
{
    if (const auto errorCode = unauthorizedErrorDetails(headers))
        return *errorCode;

    switch (statusCode)
    {
        case StatusCode::unauthorized:
            return RemoteConnectionErrorCode::unauthorized;

        case StatusCode::forbidden:
            return RemoteConnectionErrorCode::forbiddenRequest;

        default:
            return RemoteConnectionErrorCode::genericNetworkError;
    }
}

RemoteConnectionErrorCode toConnectionErrorCode(nx::cloud::db::api::ResultCode resultCode)
{
    using namespace nx::cloud::db::api;

    switch (resultCode)
    {
        case ResultCode::forbidden:
            return RemoteConnectionErrorCode::forbiddenRequest;

        case ResultCode::accountBlocked:
            return RemoteConnectionErrorCode::userIsLockedOut;

        case ResultCode::networkError:
            return RemoteConnectionErrorCode::genericNetworkError;

        default:
            return RemoteConnectionErrorCode::unauthorized;
    }
}

std::string publicKey(const nx::network::ssl::CertificateChain& chain)
{
    return (nx::network::ini().verifyVmsSslCertificates && NX_ASSERT(!chain.empty()))
        ? chain[0].publicKey() : "";
}

} // namespace

namespace detail {

struct ModuleInformationWrapper
{
    nx::vms::api::ModuleInformation reply;
};
NX_REFLECTION_INSTRUMENT(ModuleInformationWrapper, (reply));

nx::utils::Url makeUrl(nx::network::SocketAddress address, std::string path)
{
    return nx::network::url::Builder()
        .setScheme(kSecureUrlSchemeName)
        .setEndpoint(address)
        .setPath(path)
        .toUrl();
}

using Request = std::unique_ptr<AsyncClient>;

struct RemoteConnectionFactoryRequestsManager::Private
{
    QPointer<CertificateVerifier> certificateVerifier;
    std::unique_ptr<NetworkManager> networkManager = std::make_unique<NetworkManager>();

    Request makeRequestWithCertificateValidation(const std::string& expectedKey)
    {
        Request request = std::make_unique<AsyncClient>(
            (nx::network::ini().verifyVmsSslCertificates && NX_ASSERT(certificateVerifier))
                ? certificateVerifier->makeRestrictedAdapterFunc(expectedKey)
                : nx::network::ssl::kAcceptAnyCertificate);
        NetworkManager::setDefaultTimeouts(request.get());
        return request;
    }

    template<typename ResultType>
    std::function<void(NetworkManager::Response response)> makeReplyHandler(
        std::promise<ResultType>& promise,
        ContextPtr context,
        ExternalErrorHandler externalErrorHandler = {})
    {
        return
            [&promise, context, externalErrorHandler](NetworkManager::Response response)
            {
                NX_VERBOSE(NX_SCOPE_TAG, "Received reply to %1", context);
                ResultType result;
                if (!context->failed())
                {
                    if (response.statusLine.statusCode != StatusCode::undefined)
                    {
                        if (response.statusLine.statusCode == StatusCode::ok)
                        {
                            const bool parsed =
                                nx::reflect::json::deserialize(response.messageBody, &result);

                            if (!parsed)
                                context->error = RemoteConnectionErrorCode::networkContentError;
                        }
                        else
                        {
                            if (externalErrorHandler)
                                context->error = externalErrorHandler(response);

                            if (!context->error)
                            {
                                const auto errorCode = toConnectionErrorCode(
                                    response.statusLine.statusCode, response.headers);

                                nx::network::rest::Result restResult;
                                QJson::deserialize(response.messageBody, &restResult);
                                const QString externalDescription = restResult.errorString;

                                context->error = errorCode;
                                if (!externalDescription.isEmpty())
                                    context->error->externalDescription = externalDescription;
                            }

                            NX_ASSERT(context->error
                                    != RemoteConnectionErrorCode::genericNetworkError,
                                "Unprocessed error code %1, content %2",
                                response.statusLine.statusCode,
                                response.messageBody);
                        }
                    }
                    else
                    {
                        context->error = RemoteConnectionErrorCode::genericNetworkError;
                    }
                }
                promise.set_value(std::move(result));
            };
    }

    template<typename ResultType>
    ResultType doGet(
        const nx::utils::Url& url,
        ContextPtr context,
        Request request = {},
        ExternalErrorHandler externalErrorHandler = {})
    {
        if (!request)
            request = makeRequestWithCertificateValidation(publicKey(context->certificateChain));

        std::promise<ResultType> promise;
        auto future = promise.get_future();
        networkManager->doGet(
            std::move(request),
            url,
            context.get(),
            makeReplyHandler(promise, context, externalErrorHandler),
            Qt::DirectConnection);
        return future.get();
    }

    template<typename ResultType>
    ResultType doPost(
        const nx::utils::Url& url,
        ContextPtr context,
        nx::Buffer data,
        ExternalErrorHandler externalErrorHandler = {})
    {
        Request request =
            makeRequestWithCertificateValidation(publicKey(context->certificateChain));
        auto messageBody = std::make_unique<BufferSource>(
            Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
            std::move(data));
        request->setRequestBody(std::move(messageBody));

        std::promise<ResultType> promise;
        auto future = promise.get_future();
        networkManager->doPost(
            std::move(request),
            url,
            context.get(),
            makeReplyHandler(promise, context, externalErrorHandler),
            Qt::DirectConnection);
        return future.get();
    }

};

RemoteConnectionFactoryRequestsManager::RemoteConnectionFactoryRequestsManager(
    CertificateVerifier* certificateVerifier)
    :
    d(new Private)
{
    d->certificateVerifier = certificateVerifier;
}

RemoteConnectionFactoryRequestsManager::~RemoteConnectionFactoryRequestsManager()
{
    d->networkManager->pleaseStopSync();
}

void RemoteConnectionFactoryRequestsManager::fillModuleInformationAndCertificate(
    ContextPtr context)
{
    auto storeCertificateFunction =
        [context](const nx::network::ssl::CertificateChainView& chain)
        {
            context->certificateChain.clear();
            for (const auto& view: chain)
                context->certificateChain.emplace_back(view);
            return true;
        };

    NX_DEBUG(this, "Requesting module information and certificate from %1", context);
    const auto url = makeUrl(context->address(), "/api/moduleInformation");

    // Create a custom client that accepts any certificate and stores its data.
    auto request = std::make_unique<AsyncClient>(nx::network::ini().verifyVmsSslCertificates
        ? nx::network::ssl::makeAdapterFunc(storeCertificateFunction)
        : nx::network::ssl::kAcceptAnyCertificate);
    NetworkManager::setDefaultTimeouts(request.get());

    context->moduleInformation = d->doGet<ModuleInformationWrapper>(
        url, context, std::move(request)).reply;
    NX_DEBUG(this, "Received certificate chain length: %1", context->certificateChain.size());
}

nx::vms::api::LoginUser RemoteConnectionFactoryRequestsManager::getUserType(
    ContextPtr context)
{
    // This request must not be sent more often than once a second, otherwise server will reject
    // it. So we should retry until result is received.
    static constexpr auto kUserTypeRequestDelay = 1100ms;
    static constexpr int kUserTypeRequestRetryCount = 30;

    const auto encodedUsername =
        QUrl::toPercentEncoding(QString::fromStdString(context->credentials().username))
            .toStdString();

    NX_DEBUG(this, "Requesting user %1 info from %2", context->credentials().username, context);
    const auto url = makeUrl(context->address(), "/rest/v1/login/users/" + encodedUsername);

    nx::vms::api::LoginUser result;
    int retryCount = 0;
    do {
        result = d->doGet<nx::vms::api::LoginUser>(
            url,
            context,
            /*request*/ {},
            expectedErrorCodes({
                // We are forming REST api url including a user name, so `not found` in expected.
                {StatusCode::notFound, RemoteConnectionErrorCode::unauthorized}
            }));

        if (context->failed()
            && context->error == RemoteConnectionErrorCode::forbiddenRequest)
        {
            if (++retryCount >= kUserTypeRequestRetryCount)
            {
                NX_DEBUG(this, "Too many retries were unsuccessful");
                context->error = RemoteConnectionErrorCode::userIsLockedOut;
                return result;
            }

            context->error = {};
            NX_DEBUG(this, "Retry %1 in %2", retryCount, kUserTypeRequestDelay);
            std::this_thread::sleep_for(kUserTypeRequestDelay);
        }
        else
        {
            return result;
        }
    } while (!context->failed());

    return result;
}

nx::vms::api::LoginSession RemoteConnectionFactoryRequestsManager::createLocalSession(
    ContextPtr context)
{
    NX_DEBUG(this, "Creating session for user %1 on %2",
        context->credentials().username,
        context);
    const auto url = makeUrl(context->address(), "/rest/v1/login/sessions");

    nx::vms::api::LoginSessionRequest request;
    request.username = QString::fromStdString(context->credentials().username);
    request.password = QString::fromStdString(context->credentials().authToken.value);
    return d->doPost<nx::vms::api::LoginSession>(
        url,
        context,
        nx::reflect::json::serialize(request),
        expectedErrorCodes({
            {StatusCode::unprocessableEntity, RemoteConnectionErrorCode::unauthorized},
            {StatusCode::notFound, RemoteConnectionErrorCode::unauthorized} //< LDAP user.
        }));
}

nx::vms::api::LoginSession RemoteConnectionFactoryRequestsManager::getCurrentSession(
    ContextPtr context)
{
    NX_DEBUG(this, "Requesting username from %1", context);
    const auto url = makeUrl(context->address(), "/rest/v1/login/sessions/current");
    auto request = d->makeRequestWithCertificateValidation(publicKey(context->certificateChain));
    request->setCredentials(context->credentials());
    return d->doGet<nx::vms::api::LoginSession>(
        url,
        context,
        std::move(request),
        expectedErrorCodes({
            {StatusCode::unprocessableEntity, RemoteConnectionErrorCode::sessionExpired}
        }));
}

void RemoteConnectionFactoryRequestsManager::checkDigestAuthentication(ContextPtr context)
{
    NX_DEBUG(this, "Checking digest authentication as %1 in %2",
        context->credentials().username, context);

    if (context->credentials().authToken.type != AuthTokenType::password)
    {
        // It's allowed to download compatible version even if we cannot authenticate.
        if (context->logonData.purpose != LogonData::Purpose::connectInCompatibilityMode)
        {
            NX_DEBUG(this,
                "Unexpected auth token type %1",
                context->credentials().authToken.type);
            context->error = RemoteConnectionErrorCode::sessionExpired;
        }
        return;
    }

    const auto url = makeUrl(context->address(), "/api/moduleInformationAuthenticated");
    auto request = d->makeRequestWithCertificateValidation(publicKey(context->certificateChain));
    request->setCredentials(context->credentials());
    d->doGet<ModuleInformationWrapper>(url, context, std::move(request));
}

nx::cloud::db::api::IssueTokenResponse RemoteConnectionFactoryRequestsManager::issueCloudToken(
    ContextPtr context,
    nx::cloud::db::api::Connection* cloudConnection)
{
    NX_DEBUG(this, "Issue cloud token for %1 in %2", context->credentials().username, context);
    using namespace nx::cloud::db::api;

    if (!NX_ASSERT(cloudConnection))
    {
        context->error = RemoteConnectionErrorCode::cloudUnavailableOnClient;
        return {};
    }

    IssueTokenRequest request;
    request.grant_type = GrantType::refreshToken;
    request.scope =
        nx::format("cloudSystemId=%1", context->moduleInformation.cloudSystemId).toStdString();
    request.refresh_token = context->credentials().authToken.value;

    std::promise<IssueTokenResponse> promise;
    auto future = promise.get_future();
    cloudConnection->oauthManager()->issueToken(
        request,
        [&promise, context](ResultCode resultCode, IssueTokenResponse response)
        {
            NX_DEBUG(NX_SCOPE_TAG, "Issue token result: %1", resultCode);
            if (resultCode != ResultCode::ok)
                context->error = toConnectionErrorCode(resultCode);
            promise.set_value(response);
        });

    return future.get();
}

RemoteConnectionFactoryRequestsManager::ServerCertificatesInfo
    RemoteConnectionFactoryRequestsManager::targetServerCertificates(ContextPtr context)
{
    NX_DEBUG(this, "Retrieving target server certificates from %1", context);

    const auto url = makeUrl(context->address(), "/rest/v1/servers/this/info");
    auto request = d->makeRequestWithCertificateValidation(publicKey(context->certificateChain));
    auto info = d->doGet<nx::vms::api::ServerInformation>(
        url,
        context,
        std::move(request));

    ServerCertificatesInfo result;
    result.serverId = info.id;
    if (!info.certificatePem.empty())
        result.certificate = info.certificatePem;
    if (!info.userProvidedCertificatePem.empty())
        result.userProvidedCertificate = info.userProvidedCertificatePem;
    return result;
}

std::vector<RemoteConnectionFactoryRequestsManager::ServerCertificatesInfo>
    RemoteConnectionFactoryRequestsManager::pullRestCertificates(ContextPtr context)
{
    NX_DEBUG(this, "Pulling server certificates from %1", context);

    // Request full servers data, since /servers/*/info returns only online servers information.
    const auto url = makeUrl(context->address(), "/rest/v1/servers");
    auto request = d->makeRequestWithCertificateValidation(publicKey(context->certificateChain));
    request->setCredentials(context->credentials());
    auto list = d->doGet<std::vector<nx::vms::api::ServerModel>>(
        url,
        context,
        std::move(request));

    std::vector<ServerCertificatesInfo> result;

    for (const auto& serverModel: list)
    {
        ServerCertificatesInfo info;
        info.serverId = serverModel.id;

        // These fields are used by the current certificate warning dialog.
        info.serverInfo.id = serverModel.id;
        info.serverInfo.name = serverModel.name;
        info.serverUrl = serverModel.url;

        if (auto v = serverModel.parameter(ResourcePropertyKey::Server::kCertificate))
            info.certificate = v->toString().toStdString();

        NX_ASSERT(
            info.certificate || !serverModel.status
                || serverModel.status != nx::vms::api::ResourceStatus::online,
            "Server certificate is missing, server status: %1", serverModel.status);

        if (auto v = serverModel.parameter(ResourcePropertyKey::Server::kUserProvidedCertificate))
            info.userProvidedCertificate = v->toString().toStdString();

        result.push_back(std::move(info));
    }

    return result;
}

} // namespace detail
} // namespace nx::vms::client::core
