// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_factory_requests.h"

#include <chrono>
#include <memory>

#include <QtCore/QPointer>

#include <core/resource/resource_property_key.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/nx_network_ini.h>
#include <nx/network/rest/auth_result.h>
#include <nx/network/rest/result.h>
#include <nx/network/ssl/certificate.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/enum_string_conversion.h>
#include <nx/reflect/json.h>
#include <nx/utils/json/qjson.h>
#include <nx/utils/json/qt_containers_reflect.h>
#include <nx/utils/log/log.h>
#include <nx/utils/serialization/format.h>
#include <nx/vms/api/data/access_rights_data_deprecated.h>
#include <nx/vms/api/data/global_permission_deprecated.h>
#include <nx/vms/api/data/server_model.h>
#include <nx/vms/api/data/user_data_deprecated.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/utils/cloud_session_token_updater.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx_ec/ec_api_common.h>

#include "../certificate_verifier.h"
#include "../cloud_connection_factory.h"
#include "../network_manager.h"

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

static const auto kExpectedUserErrorCodes = expectedErrorCodes({
    // We are forming REST api url including a user name, so "not found" error can be
    // returned in case of server-cloud db desync.
    {StatusCode::notFound, RemoteConnectionErrorCode::networkContentError}
});

std::optional<RemoteConnectionErrorCode> unauthorizedErrorDetails(const HttpHeaders& headers)
{
    using AuthResult = nx::network::rest::AuthResult;

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

    if (authResult == AuthResult::Auth_TruncatedSessionToken)
        return RemoteConnectionErrorCode::truncatedSessionToken;

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

nx::network::ssl::AdapterFunc makeStoreCertificateFunc(
    nx::network::ssl::CertificateChain& outCertificateChain)
{
    auto storeCertificateFunction =
        [&outCertificateChain](auto chain)
        {
            for (auto&& view: chain)
                outCertificateChain.emplace_back(std::move(view));
            return true;
        };

    return nx::network::ini().verifyVmsSslCertificates
        ? nx::network::ssl::makeAdapterFunc(storeCertificateFunction)
        : nx::network::ssl::kAcceptAnyCertificate;
}

} // namespace

namespace detail {

struct ModuleInformationWrapper
{
    nx::vms::api::ModuleInformation reply;
};
NX_REFLECTION_INSTRUMENT(ModuleInformationWrapper, (reply));

nx::Url makeUrl(nx::network::SocketAddress address, std::string path = {})
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
    std::unique_ptr<CloudConnectionFactory> cloudConnectionFactory
        = std::make_unique<CloudConnectionFactory>();
    mutable std::unique_ptr<nx::cloud::db::api::Connection> cloudConnection;
    mutable nx::Mutex cloudConnectionMutex;
    std::unique_ptr<NetworkManager> networkManager = std::make_unique<NetworkManager>();

    bool ensureCloudConnection() const
    {
        NX_MUTEX_LOCKER lock(&cloudConnectionMutex);
        if (cloudConnection)
            return true;

        cloudConnection = cloudConnectionFactory->createConnection();
        return (bool) cloudConnection;
    }

    Request makeRequestWithCertificateValidation(ContextPtr context, const nx::Url& url) const
    {
        const auto expectedKey = publicKey(context->handshakeCertificateChain);
        bool lastHostIsServer = false;
        if (nx::network::ini().verifyVmsSslCertificates)
        {
            if (NX_ASSERT(!context->handshakeCertificateChain.empty()))
            {
                const auto& hosts = context->handshakeCertificateChain.front().hosts();
                lastHostIsServer =
                    !hosts.empty() && !nx::Uuid::fromStringSafe(*hosts.begin()).isNull();
            }
        }

        Request request = std::make_unique<AsyncClient>(
            (nx::network::ini().verifyVmsSslCertificates && NX_ASSERT(certificateVerifier))
                ? lastHostIsServer
                    ? certificateVerifier->makeRestrictedAdapterFunc(expectedKey)
                    : certificateVerifier->makeGeneralAdapterFunc(expectedKey, url.host().toStdString())
                : nx::network::ssl::kAcceptAnyCertificate);
        NetworkManager::setDefaultTimeouts(request.get());
        return request;
    }

    template<typename ResultType>
    std::function<void(NetworkManager::Response response)> makeReplyHandler(
        std::promise<ResultType>& promise,
        ContextPtr context,
        ExternalErrorHandler externalErrorHandler = {}) const
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
                                context->setError(RemoteConnectionErrorCode::networkContentError);
                        }
                        else
                        {
                            NX_VERBOSE(NX_SCOPE_TAG, "Status code is %1",
                                response.statusLine.statusCode);

                            if (externalErrorHandler)
                            {
                                const auto error = externalErrorHandler(response);
                                if (error)
                                    context->setError(*error);
                            }

                            if (!context->error())
                            {
                                const auto errorCode = toConnectionErrorCode(
                                    response.statusLine.statusCode, response.headers);

                                nx::network::rest::Result restResult;
                                QJson::deserialize(response.messageBody, &restResult);
                                const QString externalDescription = restResult.errorString;

                                RemoteConnectionError error(errorCode);
                                if (!externalDescription.isEmpty())
                                    error.externalDescription = externalDescription;
                                context->setError(error);
                            }

                            NX_ASSERT(context->error()
                                    != RemoteConnectionErrorCode::genericNetworkError,
                                "Unprocessed error code %1, content %2",
                                response.statusLine.statusCode,
                                response.messageBody);
                        }
                    }
                    else
                    {
                        context->setError(RemoteConnectionErrorCode::genericNetworkError);
                    }
                }
                promise.set_value(std::move(result));
            };
    }

    template<typename ResultType>
    ResultType doGet(
        const nx::Url& url,
        ContextPtr context,
        Request request = {},
        ExternalErrorHandler externalErrorHandler = {}) const
    {
        if (!request)
            request = makeRequestWithCertificateValidation(context, url);
        request->addAdditionalHeader(
            Qn::EC2_RUNTIME_GUID_HEADER_NAME, context->auditId.toSimpleStdString());

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
        const nx::Url& url,
        ContextPtr context,
        nx::Buffer data,
        ExternalErrorHandler externalErrorHandler = {}) const
    {
        Request request = makeRequestWithCertificateValidation(context, url);
        auto messageBody = std::make_unique<BufferSource>(
            Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::json),
            std::move(data));
        request->setRequestBody(std::move(messageBody));
        request->addAdditionalHeader(
            Qn::EC2_RUNTIME_GUID_HEADER_NAME, context->auditId.toSimpleStdString());

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

    Request makeRequestWithCredentials(ContextPtr context) const;
    nx::vms::api::UserModelV1 getUserModel(ContextPtr context) const;
    nx::vms::api::UserModelV1 getUserModelDeprecated(ContextPtr context) const;
};

Request RemoteConnectionFactoryRequestsManager::Private::makeRequestWithCredentials(
    ContextPtr context) const
{
    auto request = makeRequestWithCertificateValidation(context, makeUrl(context->address()));
    request->setCredentials(context->credentials());
    return request;
}

nx::vms::api::UserModelV1 RemoteConnectionFactoryRequestsManager::Private::getUserModel(
    ContextPtr context) const
{
    using namespace nx::vms::api;

    if (!NX_ASSERT(context->isRestApiSupported()))
        return {};

    NX_DEBUG(this, "Requesting user %1 model from %2", context->credentials().username, context);

    const auto encodedUsername = QUrl::toPercentEncoding(
        QString::fromStdString(context->credentials().username)).toStdString();
    const auto url = makeUrl(context->address(), "/rest/v1/users/" + encodedUsername);
    auto result = doGet<UserModelV1>(
        url, context, makeRequestWithCredentials(context), kExpectedUserErrorCodes);

    if (context->failed())
        return {};

    if (!result.userRoleId.isNull())
    {
        NX_DEBUG(this, "Requesting user role model for %1 from %2",
            context->credentials().username, context);

        const auto url = makeUrl(
            context->address(), "/rest/v1/userRoles/" + result.userRoleId.toSimpleStdString());
        const auto role = doGet<UserRoleModel>(url, context, makeRequestWithCredentials(context));
        if (context->failed())
            return {};

        // As we don't really need compatibility mode for the desktop client, it is enough
        // to just copy permissions and accessible resources to user.
        result.permissions = role.permissions;
        result.accessibleResources = role.accessibleResources;
    }
    return result;
}

nx::vms::api::UserModelV1 RemoteConnectionFactoryRequestsManager::Private::getUserModelDeprecated(
    ContextPtr context) const
{
    using namespace nx::vms::api;

    NX_DEBUG(this, "Requesting deprecated user %1 model from %2", context->credentials().username,
        context);

    const auto url = makeUrl(context->address(), "/ec2/getUsers");
    const auto users = doGet<UserDataDeprecatedList>(
        url, context, makeRequestWithCredentials(context), kExpectedUserErrorCodes);

    if (context->failed())
        return {};

    const auto userIt = std::find_if(users.cbegin(), users.cend(),
        [userName = context->credentials().username](const auto& user)
        {
            return user.name.toLower() == userName;
        });

    if (userIt == users.cend())
    {
        context->setError(RemoteConnectionErrorCode::unauthorized);
        return {};
    }

    UserModelV1 result;
    result.id = userIt->id;
    result.isOwner = userIt->isAdmin;
    result.name = userIt->name;
    result.permissions = userIt->permissions;
    result.userRoleId = userIt->userRoleId;
    result.email = userIt->email;
    result.fullName = userIt->fullName;
    result.type = userIt->isCloud
        ? UserType::cloud
        : (userIt->isLdap ? UserType::ldap : UserType::local);

    {
        // Request available resources.

        const auto url = makeUrl(context->address(), "/ec2/getAccessRights");
        const auto accessRights = doGet<AccessRightsDataDeprecatedList>(
            url, context, makeRequestWithCredentials(context));
        if (context->failed())
            return {};

        const auto itRights = std::find_if(accessRights.cbegin(), accessRights.cend(),
            [result](const auto& rights)
            {
                return rights.userId == result.id || rights.userId == result.userRoleId;
            });

        if (itRights != accessRights.cend())
            result.accessibleResources = itRights->resourceIds;
    }

    if (!result.userRoleId.isNull())
    {
        // Request permissions for custom role.

        const auto url = makeUrl(context->address(), "/ec2/getUserRoles");
        const auto roles = doGet<UserRoleModelList>(
            url, context, makeRequestWithCredentials(context));
        if (context->failed())
            return {};

        const auto itRole = std::find_if(roles.cbegin(), roles.cend(),
            [roleId = result.userRoleId](const auto& role)
            {
                return roleId == role.id;
            });

        if (itRole == roles.cend())
        {
            context->setError(RemoteConnectionErrorCode::unauthorized);
            return {};
        }

        result.permissions = itRole->permissions;
    }

    return result;
}

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

RemoteConnectionFactoryRequestsManager::ModuleInformationReply
    RemoteConnectionFactoryRequestsManager::getModuleInformation(ContextPtr context) const
{
    NX_DEBUG(this, "Requesting module information and certificate from %1", context);
    const auto url = makeUrl(context->address(), "/api/moduleInformation");

    const bool certificatePresent = (context->handshakeCertificateChain.size() > 0);

    // Create a custom client that accepts any certificate and stores its data.
    ModuleInformationReply reply{.handshakeCertificateChain = context->handshakeCertificateChain};
    auto request = certificatePresent
        ? d->makeRequestWithCertificateValidation(context, url)
        : std::make_unique<AsyncClient>(makeStoreCertificateFunc(reply.handshakeCertificateChain));
    NetworkManager::setDefaultTimeouts(request.get());

    reply.moduleInformation =
        d->doGet<ModuleInformationWrapper>(url, context, std::move(request)).reply;

    if (!context->failed())
    {
        NX_DEBUG(this, "Received module information for server %1", reply.moduleInformation.id);
        NX_VERBOSE(this, "Data:\n%1", nx::reflect::json::serialize(reply.moduleInformation));
        if (reply.moduleInformation.id.isNull())
        {
            NX_WARNING(this, "Received module information is invalid");
            context->setError(RemoteConnectionErrorCode::networkContentError);
        }
    }

    NX_DEBUG(this, "Stored certificate chain length: %1", reply.handshakeCertificateChain.size());

    return reply;
}

RemoteConnectionFactoryRequestsManager::ServersInfoReply
    RemoteConnectionFactoryRequestsManager::getServersInfo(ContextPtr context) const
{
    static const nx::utils::SoftwareVersion kServersInfoV2Supported{5, 1, 0, 0};

    NX_DEBUG(this, "Retrieving list of servers info from %1", context);

    const auto url =
        [context]()
        {
            if (!NX_ASSERT(context->expectedServerVersion())
                || *context->expectedServerVersion() < kServersInfoV2Supported)
            {
                return makeUrl(context->address(), "/rest/v1/servers/*/info");
            }

            auto url = makeUrl(context->address(), "/rest/v2/servers/*/info");
            QUrlQuery query;
            query.addQueryItem("onlyFreshInfo", "false");
            url.setQuery(query);
            return url;
        }();

    const bool certificatePresent = (context->handshakeCertificateChain.size() > 0);

    // Create a custom client that accepts any certificate and stores its data.
    ServersInfoReply reply{.handshakeCertificateChain = context->handshakeCertificateChain};
    auto request = certificatePresent
        ? d->makeRequestWithCertificateValidation(context, url)
        : std::make_unique<AsyncClient>(makeStoreCertificateFunc(reply.handshakeCertificateChain));
    NetworkManager::setDefaultTimeouts(request.get());

    reply.serversInfo =
        d->doGet<std::vector<nx::vms::api::ServerInformationV1>>(url, context, std::move(request));

    if (!context->failed())
    {
        NX_DEBUG(this, "Received list of %1 servers", reply.serversInfo.size());
        NX_VERBOSE(this, "Data:\n%1", nx::reflect::json::serialize(reply.serversInfo));
    }

    NX_DEBUG(this, "Stored certificate chain length: %1", reply.handshakeCertificateChain.size());

    return reply;
}

nx::vms::api::UserModelV1 RemoteConnectionFactoryRequestsManager::getUserModel(
    ContextPtr context) const
{
    return context->isRestApiSupported()
        ? d->getUserModel(context)
        : d->getUserModelDeprecated(context);
}

nx::vms::api::LoginUser RemoteConnectionFactoryRequestsManager::getUserType(
    ContextPtr context) const
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
            && context->error() == RemoteConnectionErrorCode::forbiddenRequest)
        {
            if (++retryCount >= kUserTypeRequestRetryCount)
            {
                NX_DEBUG(this, "Too many retries were unsuccessful");
                context->rewriteError(RemoteConnectionErrorCode::userIsLockedOut);
                return result;
            }

            context->resetError();
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
    ContextPtr context) const
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

nx::vms::api::LoginSession RemoteConnectionFactoryRequestsManager::createTemporaryLocalSession(
    ContextPtr context) const
{
    NX_DEBUG(this, "Creating temporary session for user %1 on %2",
        context->credentials().username,
        context);

    const auto url = makeUrl(context->address(), "/rest/v3/login/temporaryToken");

    nx::vms::api::TemporaryLoginSessionRequest request;
    request.token = QString::fromStdString(context->credentials().authToken.value);
    return d->doPost<nx::vms::api::LoginSession>(
        url,
        context,
        nx::reflect::json::serialize(request),
        expectedErrorCodes({
            {StatusCode::unprocessableEntity, RemoteConnectionErrorCode::unauthorized},
            {StatusCode::badRequest, RemoteConnectionErrorCode::unauthorized}
        }));
}

nx::vms::api::LoginSession RemoteConnectionFactoryRequestsManager::getCurrentSession(
    ContextPtr context) const
{
    NX_DEBUG(this, "Requesting username from %1", context);
    const auto url = makeUrl(context->address(), "/rest/v1/login/sessions/current");

    ExternalErrorMap expectedErrors{
        {StatusCode::unprocessableEntity, RemoteConnectionErrorCode::sessionExpired},
        // When one server has not cached the session token and cannot validate the token on the
        // second server, ServiceUnavailable is returned. On the client side, this situation should
        // be interpreted as an expired session token.
        {StatusCode::serviceUnavailable, RemoteConnectionErrorCode::sessionExpired}
    };

    if (context->isCloudConnection())
    {
        using namespace nx::vms::common;

        expectedErrors[StatusCode::serviceUnavailable] =
            RemoteConnectionErrorCode::cloudUnavailableOnServer;

        if (context->moduleInformation.isSaasSystem()
            && saas::ServiceManager::saasSuspendedOrShutDown(context->moduleInformation.saasState))
        {
            expectedErrors[StatusCode::unprocessableEntity] =
                RemoteConnectionErrorCode::loginAsCloudUserForbidden;
        }
    }

    return d->doGet<nx::vms::api::LoginSession>(
        url,
        context,
        d->makeRequestWithCredentials(context),
        expectedErrorCodes(expectedErrors));
}

void RemoteConnectionFactoryRequestsManager::checkDigestAuthentication(
    ContextPtr context, bool is2FaEnabledForUser) const
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
            context->setError(RemoteConnectionErrorCode::sessionExpired);
        }
        return;
    }

    const auto expectedErrors = expectedErrorCodes(
        [context, is2FaEnabledForUser]() -> ExternalErrorMap
        {
            if (is2FaEnabledForUser
                && context->logonData.purpose != LogonData::Purpose::connectInCompatibilityMode
                && context->userType() == nx::vms::api::UserType::cloud
                && !context->isRestApiSupported())
            {
                using Code = RemoteConnectionErrorCode;
                return {{StatusCode::unauthorized, Code::systemIsNotCompatibleWith2Fa}};
            }
            return {};
        }());

    const auto url = makeUrl(context->address(), "/api/moduleInformationAuthenticated");
    d->doGet<ModuleInformationWrapper>(
        url, context, d->makeRequestWithCredentials(context), expectedErrors);
}

std::future<RemoteConnectionFactoryContext::CloudTokenInfo>
    RemoteConnectionFactoryRequestsManager::issueCloudToken(
        ContextPtr context,
        const QString& cloudSystemId) const
{
    NX_DEBUG(this, "Issue cloud token for %1 in %2", context->credentials().username, context);
    using namespace nx::cloud::db::api;

    if (!d->ensureCloudConnection())
    {
        context->setError(RemoteConnectionErrorCode::cloudUnavailableOnClient);
        return {};
    }

    if (!NX_ASSERT(!cloudSystemId.isEmpty()))
    {
        context->setError(RemoteConnectionErrorCode::internalError);
        return {};
    }

    IssueTokenRequest request;
    request.client_id = CloudConnectionFactory::clientId();
    request.grant_type = GrantType::refresh_token;
    request.scope =
        nx::format("cloudSystemId=%1", cloudSystemId).toStdString();
    request.refresh_token = context->credentials().authToken.value;

    auto promise = std::make_unique<std::promise<Context::CloudTokenInfo>>();
    auto future = promise->get_future();
    auto handler = [promise = std::move(promise)](ResultCode resultCode, IssueTokenResponse response)
        {
            NX_DEBUG(NX_SCOPE_TAG, "Issue token result: %1", resultCode);
            Context::CloudTokenInfo tokenInfo{.response = response};
            if (resultCode != ResultCode::ok)
                tokenInfo.error = toConnectionErrorCode(resultCode);
            promise->set_value(tokenInfo);
        };
    if (ini().useShortLivedCloudTokens)
        d->cloudConnection->oauthManager()->issueTokenV1(request, std::move(handler));
    else
        d->cloudConnection->oauthManager()->issueTokenLegacy(request, std::move(handler));

    return future;
}

} // namespace detail
} // namespace nx::vms::client::core
