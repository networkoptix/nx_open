// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_factory.h"

#include <memory>

#include <QtCore/QPointer>

#include <network/system_helpers.h>
#include <nx/cloud/db/api/oauth_manager.h>
#include <nx/network/address_resolver.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/nx_network_ini.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/to_string.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/thread/thread_util.h>
#include <nx/vms/api/data/login.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx/vms/common/resource/server_host_priority.h>
#include <nx_ec/ec_api_fwd.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>
#include <utils/email/email.h>

#include "abstract_remote_connection_factory_requests.h"
#include "certificate_verifier.h"
#include "network_manager.h"
#include "remote_connection.h"
#include "remote_connection_user_interaction_delegate.h"

using namespace ec2;
using ServerCompatibilityValidator = nx::vms::common::ServerCompatibilityValidator;

namespace nx::vms::client::core {

namespace {

static const nx::utils::SoftwareVersion kRestApiSupportVersion(5, 0);
static const nx::utils::SoftwareVersion kSimplifiedLoginSupportVersion(5, 1);
static const nx::utils::SoftwareVersion kUserRightsRedesignVersion(6, 0);

/**
 * Digest authentication requires username to be in lowercase.
 */
void ensureUserNameIsLowercaseIfDigest(nx::network::http::Credentials& credentials)
{
    if (credentials.authToken.isPassword())
    {
        const QString username = QString::fromStdString(credentials.username);
        credentials.username = username.toLower().toStdString();
    }
}

RemoteConnectionErrorCode toErrorCode(ServerCompatibilityValidator::Reason reason)
{
    switch (reason)
    {
        case ServerCompatibilityValidator::Reason::binaryProtocolVersionDiffers:
            return RemoteConnectionErrorCode::binaryProtocolVersionDiffers;

        case ServerCompatibilityValidator::Reason::cloudHostDiffers:
            return RemoteConnectionErrorCode::cloudHostDiffers;

        case ServerCompatibilityValidator::Reason::customizationDiffers:
            return RemoteConnectionErrorCode::customizationDiffers;

        case ServerCompatibilityValidator::Reason::versionIsTooLow:
            return RemoteConnectionErrorCode::versionIsTooLow;
    }

    NX_ASSERT(false, "Should never get here");
    return RemoteConnectionErrorCode::internalError;
}

std::optional<std::string> publicKey(const std::string& pem)
{
    if (pem.empty())
        return {};

    auto chain = nx::network::ssl::Certificate::parse(pem);
    if (chain.empty())
        return {};

    return chain[0].publicKey();
};

std::optional<QnUuid> deductServerId(const std::vector<nx::vms::api::ServerInformation>& info)
{
    if (info.size() == 1)
        return info.begin()->id;

    for (const auto& server: info)
    {
        if (server.collectedByThisServer)
            return server.id;
    }

    return std::nullopt;
}

nx::utils::Url mainServerUrl(const QSet<QString>& remoteAddresses, int port)
{
    std::vector<nx::utils::Url> addresses;
    for (const auto& addressString: remoteAddresses)
    {
        nx::network::SocketAddress sockAddr(addressString.toStdString());

        nx::utils::Url url = nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName)
            .setEndpoint(sockAddr)
            .toUrl();
        if (url.port() == 0)
            url.setPort(port);

        addresses.push_back(url);
    }

    if (addresses.empty())
        return {};

    return *std::min_element(addresses.cbegin(), addresses.cend(),
        [](const auto& l, const auto& r)
        {
            using namespace nx::vms::common;
            return serverHostPriority(l.host()) < serverHostPriority(r.host());
        });
}

} // namespace

using WeakContextPtr = std::weak_ptr<RemoteConnectionFactory::Context>;

struct RemoteConnectionFactory::Private
{
    using RequestsManager = AbstractRemoteConnectionFactoryRequestsManager;

    ec2::AbstractECConnectionFactory* q;
    const nx::vms::api::PeerType peerType;
    const Qn::SerializationFormat serializationFormat;
    QPointer<CertificateVerifier> certificateVerifier;
    AuditIdProvider auditIdProvider;
    CloudCredentialsProvider cloudCredentialsProvider;
    std::shared_ptr<RequestsManager> requestsManager;
    std::unique_ptr<AbstractRemoteConnectionUserInteractionDelegate> userInteractionDelegate;

    Private(
        ec2::AbstractECConnectionFactory* q,
        nx::vms::api::PeerType peerType,
        Qn::SerializationFormat serializationFormat,
        CertificateVerifier* certificateVerifier,
        AuditIdProvider auditIdProvider,
        CloudCredentialsProvider cloudCredentialsProvider,
        std::shared_ptr<RequestsManager> requestsManager)
        :
        q(q),
        peerType(peerType),
        serializationFormat(serializationFormat),
        certificateVerifier(certificateVerifier),
        auditIdProvider(std::move(auditIdProvider)),
        cloudCredentialsProvider(std::move(cloudCredentialsProvider)),
        requestsManager(std::move(requestsManager))
    {
        NX_ASSERT(this->auditIdProvider);
        NX_ASSERT(this->cloudCredentialsProvider.getCredentials);
        NX_ASSERT(this->cloudCredentialsProvider.getLogin);
        NX_ASSERT(this->cloudCredentialsProvider.getDigestPassword);
        NX_ASSERT(this->cloudCredentialsProvider.is2FaEnabledForUser);
    }

    RemoteConnectionPtr makeRemoteConnectionInstance(ContextPtr context)
    {
        ConnectionInfo connectionInfo{
            .address = context->address(),
            .credentials = context->credentials(),
            .userType = context->userType(),
            .compatibilityUserModel = context->compatibilityUserModel};

        return std::make_shared<RemoteConnection>(
            peerType,
            context->moduleInformation,
            connectionInfo,
            context->sessionTokenExpirationTime,
            context->certificateCache,
            serializationFormat,
            /*auditId*/ auditIdProvider());
    }

    bool executeInUiThreadSync(
        ContextPtr context,
        std::function<bool(AbstractRemoteConnectionUserInteractionDelegate* delegate)> handler)
    {
        using namespace std::chrono_literals;

        std::promise<bool> isAccepted;
        auto delegate = context->customUserInteractionDelegate
            ? context->customUserInteractionDelegate.get()
            : userInteractionDelegate.get();
        executeInThread(delegate->thread(),
            [&isAccepted, handler, delegate]()
            {
                isAccepted.set_value(handler(delegate));
            });

        std::future<bool> future = isAccepted.get_future();
        std::future_status status;
        do {
            status = future.wait_for(100ms);
            if (context->terminated)
                return false;

        } while (status != std::future_status::ready);

        return future.get();
    }

    void logInitialInfo(ContextPtr context)
    {
        NX_DEBUG(this, "Initialize connection process.");
        if (!NX_ASSERT(context))
            return;

        NX_DEBUG(this, "Url %1, purpose %2",
            context->logonData.address,
            nx::reflect::toString(context->logonData.purpose));

        if (context->expectedServerId())
            NX_DEBUG(this, "Expect Server ID %1.", *context->expectedServerId());
        else
            NX_DEBUG(this, "Server ID is not known.");

        if (context->expectedServerVersion())
            NX_DEBUG(this, "Expect Server version %1.", *context->expectedServerVersion());
        else
            NX_DEBUG(this, "Server version is not known.");

        if (context->isCloudConnection())
        {
            if (context->expectedCloudSystemId())
                NX_DEBUG(this, "Expect Cloud connect to %1.", *context->expectedCloudSystemId());
            else
                NX_ASSERT(this, "Expect Cloud connect but the System ID is not known yet.");
        }
    }

    bool needToRequestModuleInformationFirst(ContextPtr context) const
    {
        if (!context)
            return false;

        return !context->expectedServerVersion()
            || *context->expectedServerVersion() < kRestApiSupportVersion;
    };

    /** Check whether System supports REST API by it's version (which must be known already). */
    bool systemSupportsRestApi(ContextPtr context) const
    {
        if (!context)
            return false;

        if (!context->expectedServerVersion())
        {
            context->setError(RemoteConnectionErrorCode::internalError);
            return false;
        }

        return *context->expectedServerVersion() >= kRestApiSupportVersion;
    }

    /**
     * Validate expected Server ID and store received info in the context.
     */
    void getModuleInformation(ContextPtr context) const
    {
        if (!context)
            return;

        const auto reply = requestsManager->getModuleInformation(context);
        if (context->failed())
            return;

        NX_DEBUG(this, "Fill context info from module information reply.");
        context->handshakeCertificateChain = reply.handshakeCertificateChain;
        context->moduleInformation = reply.moduleInformation;

        // Check whether actual server id matches the one we expected to get (if any).
        if (context->expectedServerId()
            && context->expectedServerId() != context->moduleInformation.id)
        {
            context->setError(RemoteConnectionErrorCode::unexpectedServerId);
            return;
        }

        context->logonData.expectedServerId = reply.moduleInformation.id;
        context->logonData.expectedServerVersion = reply.moduleInformation.version;
        context->logonData.expectedCloudSystemId = reply.moduleInformation.cloudSystemId;

        if (reply.moduleInformation.id.isNull()
            || reply.moduleInformation.version.isNull())
        {
            context->setError(RemoteConnectionErrorCode::networkContentError);
        }
    }

    void getServersInfo(ContextPtr context)
    {
        if (!context)
            return;

        const auto reply = requestsManager->getServersInfo(context);
        if (context->failed())
            return;

        NX_DEBUG(this, "Fill context info from servers info reply.");
        context->handshakeCertificateChain = reply.handshakeCertificateChain;
        context->serversInfo = reply.serversInfo;
        if (reply.serversInfo.empty())
        {
            context->setError(RemoteConnectionErrorCode::networkContentError);
            return;
        }

        if (!context->expectedServerId())
        {
            // Try to deduct server id for the 5.1 Systems or Systems with one server.
            context->logonData.expectedServerId = deductServerId(reply.serversInfo);
        }

        if (!context->expectedCloudSystemId())
        {
            NX_DEBUG(this, "Fill Cloud System ID from servers info");
            context->logonData.expectedCloudSystemId = reply.serversInfo[0].cloudSystemId;
        }
    }

    void ensureExpectedServerId(ContextPtr context)
    {
        if (!context || context->expectedServerId())
            return;

        // We can connect to 5.0 System without knowing actual server id, so we need to get it.
        NX_DEBUG(this, "Cannot deduct Server ID, requesting it additionally.");

        // GET /api/moduleInformation.
        const auto reply = requestsManager->getModuleInformation(context);
        if (!context->failed())
            context->logonData.expectedServerId = reply.moduleInformation.id;
    }

    void processServersInfoList(ContextPtr context)
    {
        if (!context)
            return;

        if (!NX_ASSERT(context->expectedServerId()))
        {
            context->setError(RemoteConnectionErrorCode::internalError);
            return;
        }

        auto currentServerIt = std::find_if(
            context->serversInfo.cbegin(),
            context->serversInfo.cend(),
            [id = *context->expectedServerId()](const auto& server) { return server.id == id; });

        if (currentServerIt == context->serversInfo.cend())
        {
            NX_WARNING(
                this,
                "Server info list does not contain Server %1.",
                *context->expectedServerId());
            context->setError(RemoteConnectionErrorCode::networkContentError);
            return;
        }

        context->moduleInformation = *currentServerIt;

        bool certificateIsUserProvided = false;

        // Check that the handshake certificate matches one of the targets's.
        CertificateVerificationResult certificateVerificationResult =
            verifyHandshakeCertificateChain(context->handshakeCertificateChain, *currentServerIt);
        if (!certificateVerificationResult.success)
        {
            context->setError(RemoteConnectionErrorCode::certificateRejected);
            return;
        }

        // User interaction.
        verifyTargetCertificate(context, certificateVerificationResult.isUserProvidedCertificate);
        if (!context->failed())
            processCertificates(context);
    }

    bool checkCompatibility(ContextPtr context)
    {
        if (context)
        {
            NX_DEBUG(this, "Checking compatibility");
            if (const auto incompatibilityReason = ServerCompatibilityValidator::check(
                context->moduleInformation, context->logonData.purpose))
            {
                context->setError(toErrorCode(*incompatibilityReason));
            }
            else if (context->moduleInformation.isNewSystem())
            {
                context->setError(RemoteConnectionErrorCode::factoryServer);
            }
        }
        return context && !context->failed();
    }

    void verifyTargetCertificate(ContextPtr context, bool certificateIsUserProvided)
    {
        if (!context || !nx::network::ini().verifyVmsSslCertificates)
            return;

        NX_DEBUG(this, "Verify certificate");
        // Make sure factory is setup correctly.
        if (!NX_ASSERT(certificateVerifier)
            || !NX_ASSERT(userInteractionDelegate))
        {
            context->setError(RemoteConnectionErrorCode::internalError);
            return;
        }

        const CertificateVerifier::Status status = certificateVerifier->verifyCertificate(
            context->moduleInformation.id,
            context->handshakeCertificateChain);

        if (status == CertificateVerifier::Status::ok)
            return;

        auto pinTargetServerCertificate =
            [this, context, certificateIsUserProvided]
            {
                if (!NX_ASSERT(!context->handshakeCertificateChain.empty()))
                    return;

                // Server certificates are checked in checkServerCertificateEquality() using
                // REST API. After that we know exactly which type of certificate is used on
                // SSL handshake. For old servers that don't support REST API the certificate
                // is stored as auto-generated.
                certificateVerifier->pinCertificate(
                    context->moduleInformation.id,
                    context->handshakeCertificateChain[0].publicKey(),
                    certificateIsUserProvided
                        ? CertificateVerifier::CertificateType::connection
                        : CertificateVerifier::CertificateType::autogenerated);
            };

        const auto serverName = context->address().address.toString();
        NX_DEBUG(
            this,
            "New certificate has been received from %1. "
            "Trying to verify it by system CA certificates.",
            serverName);

        std::string errorMessage;
        if (NX_ASSERT(!context->handshakeCertificateChain.empty())
            && nx::network::ssl::verifyBySystemCertificates(
                context->handshakeCertificateChain, serverName, &errorMessage))
        {
            NX_DEBUG(this, "Certificate verification for %1 is successful.", serverName);
            pinTargetServerCertificate();
            return;
        }

        NX_DEBUG(this, errorMessage);

        auto accept =
            [status, context](AbstractRemoteConnectionUserInteractionDelegate* delegate)
            {
                if (status == CertificateVerifier::Status::notFound)
                {
                    return delegate->acceptNewCertificate(
                        context->moduleInformation,
                        context->address(),
                        context->handshakeCertificateChain);
                }
                else if (status == CertificateVerifier::Status::mismatch)
                {
                    return delegate->acceptCertificateAfterMismatch(
                        context->moduleInformation,
                        context->address(),
                        context->handshakeCertificateChain);
                }
                return false;
            };

        if (auto accepted = context->logonData.userInteractionAllowed
            && executeInUiThreadSync(context, accept))
        {
            pinTargetServerCertificate();
        }
        else
        {
            // User rejected the certificate.
            context->setError(RemoteConnectionErrorCode::certificateRejected);
        }
    }

    void fillCloudConnectionCredentials(ContextPtr context,
        const nx::utils::SoftwareVersion& serverVersion)
    {
        if (!NX_ASSERT(context))
            return;

        if (!NX_ASSERT(context->userType() == nx::vms::api::UserType::cloud))
            return;

        auto& credentials = context->logonData.credentials;

        // Use refresh token to issue new session token if server supports OAuth cloud
        // authorization through the REST API.
        if (serverVersion >= kRestApiSupportVersion)
        {
            credentials = cloudCredentialsProvider.getCredentials();
        }
        // Current or stored credentials will be passed to compatibility mode client.
        else if (credentials.authToken.empty())
        {
            context->logonData.credentials.username =
                cloudCredentialsProvider.getLogin();
            if (!context->logonData.credentials.authToken.isPassword()
                || context->logonData.credentials.authToken.value.empty())
            {
                context->logonData.credentials.authToken =
                    nx::network::http::PasswordAuthToken(
                        cloudCredentialsProvider.getDigestPassword());
            }
        }
    }

    /**
     * For cloud connections we can get url, containing only system id. In that case each request
     * may potentially be sent to another server, which may lead to undefined behavior. So we are
     * replacing generic url with the exact server's one.
     */
    void pinCloudConnectionAddressIfNeeded(ContextPtr context)
    {
        if (!context || !context->isCloudConnection())
            return;

        const std::string hostname = context->address().address.toString();
        if (nx::network::SocketGlobals::addressResolver().isCloudHostname(hostname))
        {
            const auto fullHostname = context->moduleInformation.id.toSimpleString()
                + '.'
                + context->moduleInformation.cloudSystemId;
            context->logonData.address.address = fullHostname;
            NX_DEBUG(this, "Fixed connection address: %1", fullHostname);
        }
    }

    void checkServerCloudConnection(ContextPtr context)
    {
        if (!context)
            return;

        requestsManager->getCurrentSession(context);

        // Assuming server cloud connection problems if fresh cloud token is unauthorized
        if (context->failed() && context->error() == RemoteConnectionErrorCode::sessionExpired)
            context->rewriteError(RemoteConnectionErrorCode::cloudUnavailableOnServer);
    }

    bool isRestApiSupported(ContextPtr context)
    {
        if (!context)
            return false;

        if (!context->moduleInformation.version.isNull())
            return context->moduleInformation.version >= kRestApiSupportVersion;

        return context->expectedServerVersion()
            && *context->expectedServerVersion() >= kRestApiSupportVersion;
    }

    bool isSystemCompatibleWithUser(ContextPtr context)
    {
        if (!context)
            return false;

        NX_DEBUG(this, "Checking System compatibility with the User.");

        // The system version below 5.0 is not compatible with a cloud user with 2fa enabled, but
        // we still can download compatible client.
        if (!isRestApiSupported(context)
            && context->logonData.purpose != LogonData::Purpose::connectInCompatibilityMode
            && context->userType() == nx::vms::api::UserType::cloud
            && cloudCredentialsProvider.is2FaEnabledForUser())
        {
            context->setError(RemoteConnectionErrorCode::systemIsNotCompatibleWith2Fa);
            return false;
        }

        return true;
    }

    /**
     * Systems before 6.0 use old permissions model, where resource access is controlled by global
     * permissions and there is a special `isAdmin` field.
     */
    bool isOldPermissionsModelUsed(ContextPtr context)
    {
        if (!context)
            return false;

        if (!context->moduleInformation.version.isNull())
            return context->moduleInformation.version < kUserRightsRedesignVersion;

        return context->expectedServerVersion()
            && *context->expectedServerVersion() < kUserRightsRedesignVersion;
    }

    void loginWithDigest(ContextPtr context)
    {
        if (context)
        {
            ensureUserNameIsLowercaseIfDigest(context->logonData.credentials);
            requestsManager->checkDigestAuthentication(context);
        }
    }

    bool canLoginWithSavedToken(ContextPtr context) const
    {
        return context
            && context->credentials().authToken.isBearerToken()
            && !context->credentials().authToken.value.starts_with(api::TemporaryToken::kPrefix);
    }

    void loginWithToken(ContextPtr context)
    {
        if (!context)
            return;

        // In case token is outdated we will receive unauthorized here.
        nx::vms::api::LoginSession currentSession = requestsManager->getCurrentSession(context);
        if (!context->failed())
        {
            context->logonData.credentials.username = currentSession.username.toStdString();
            context->sessionTokenExpirationTime =
                qnSyncTime->currentTimePoint() + currentSession.expiresInS;
        }
    }

    nx::vms::api::LoginUser verifyLocalUserType(ContextPtr context)
    {
        if (context)
        {
            // Cloud users are not allowed to be here.
            if (!NX_ASSERT(context->userType() != nx::vms::api::UserType::cloud))
            {
                context->setError(RemoteConnectionErrorCode::internalError);
                return {};
            }

            // Username may be not passed if logging in as local or ldap user by token.
            if (context->credentials().username.empty())
            {
                if (const auto token = context->credentials().authToken; token.isBearerToken())
                {
                    if (token.value.starts_with(nx::vms::api::LoginSession::kTokenPrefix))
                        return {.type = context->logonData.userType};

                    if (token.value.starts_with(nx::vms::api::TemporaryToken::kPrefix))
                    {
                        context->logonData.userType = nx::vms::api::UserType::temporaryLocal;
                        return {.type = context->logonData.userType};
                    }
                }
            }

            nx::vms::api::LoginUser loginUserData = requestsManager->getUserType(context);
            if (context->failed())
                return {};

            // Check if expected user type does not match actual. Possible scenarios:
            // * Receive cloud user using the local tile - forbidden, throw error.
            // * Receive ldap user using the local tile - OK, updating actual user type.
            switch (loginUserData.type)
            {
                case nx::vms::api::UserType::cloud:
                    context->setError(RemoteConnectionErrorCode::loginAsCloudUserForbidden);
                    break;
                case nx::vms::api::UserType::ldap:
                    context->logonData.userType = nx::vms::api::UserType::ldap;
                    break;
                default:
                    break;
            }
            return loginUserData;
        }
        return {};
    }

    void processCloudToken(ContextPtr context)
    {
        if (!context || context->failed())
            return;

        NX_DEBUG(this, "Connecting as Cloud User, waiting for the access token.");
        if (!context->cloudToken.valid()) //< Cloud token request must be sent already.
        {
            context->setError(RemoteConnectionErrorCode::internalError);
            return;
        }

        const auto cloudTokenInfo = context->cloudToken.get();
        if (cloudTokenInfo.error)
        {
            context->setError(*cloudTokenInfo.error);
            return;
        }

        const auto& response = cloudTokenInfo.response;
        if (response.error)
        {
            NX_DEBUG(this, "Token response error: %1", *response.error);
            if (*response.error == nx::cloud::db::api::OauthManager::k2faRequiredError)
            {
                auto credentials = context->credentials();
                credentials.authToken =
                    nx::network::http::BearerAuthToken(response.access_token);

                auto validate =
                    [credentials = std::move(credentials)]
                        (AbstractRemoteConnectionUserInteractionDelegate* delegate)
                    {
                        return delegate->request2FaValidation(credentials);
                    };

                const bool validated = context->logonData.userInteractionAllowed
                    && executeInUiThreadSync(context, validate);

                if (validated)
                {
                    context->logonData.credentials.authToken =
                        nx::network::http::BearerAuthToken(response.access_token);
                    context->sessionTokenExpirationTime = response.expires_at;
                }
                else
                {
                    context->setError(RemoteConnectionErrorCode::unauthorized);
                }
            }
            else if (*response.error
                == nx::cloud::db::api::OauthManager::k2faDisabledForUserError)
            {
                context->setError(
                    RemoteConnectionErrorCode::twoFactorAuthOfCloudUserIsDisabled);
            }
            else
            {
                context->setError(RemoteConnectionErrorCode::unauthorized);
            }
        }
        else
        {
            NX_DEBUG(this, "Access token received successfully.");
            context->logonData.credentials.authToken =
                nx::network::http::BearerAuthToken(response.access_token);
            context->sessionTokenExpirationTime = response.expires_at;
        }
    }

    void requestCloudTokenIfPossible(ContextPtr context)
    {
        if (context
            && context->isCloudConnection() //< Actual for the Cloud User only.
            && context->expectedServerVersion() //< Required to get the credentials correctly.
            && context->expectedCloudSystemId() //< Required to set token scope.
            && !context->expectedCloudSystemId()->isEmpty()
            && !context->cloudToken.valid()) //< Whether token is already requested.
        {
            NX_DEBUG(this, "Requesting Cloud access token.");

            fillCloudConnectionCredentials(context, *context->expectedServerVersion());
            // GET /cdb/oauth2/token.
            context->cloudToken = requestsManager->issueCloudToken(
                context,
                *context->expectedCloudSystemId());
        }
    }

    void issueLocalToken(ContextPtr context)
    {
        if (!context)
            return;

        const bool isTemporaryUser =
            context->logonData.userType == nx::vms::api::UserType::temporaryLocal;

        nx::vms::api::LoginSession session = isTemporaryUser
            ? requestsManager->createTemporaryLocalSession(context)
            : requestsManager->createLocalSession(context);

        if (!context->failed())
        {
            context->logonData.credentials.username = session.username.toStdString();
            context->logonData.credentials.authToken =
                nx::network::http::BearerAuthToken(session.token);
            context->sessionTokenExpirationTime =
                qnSyncTime->currentTimePoint() + session.expiresInS;
        }
    }

    void emulateCompatibilityUserModel(ContextPtr context)
    {
        if (!context)
            return;

        context->compatibilityUserModel = nx::vms::api::UserModelV1();
        context->compatibilityUserModel->isOwner = true;
    }

    void requestCompatibilityUserPermissions(ContextPtr context)
    {
        if (!context)
            return;

        if (!NX_ASSERT(!context->credentials().username.empty()))
            return;

        nx::vms::api::UserModelV1 userModel = requestsManager->getUserModel(context);
        if (!context->failed())
        {
            NX_VERBOSE(this, "Compatibility user model received");
            if (userModel.permissions.testFlag(
                nx::vms::api::GlobalPermissionDeprecated::customUser))
            {
                // Requesting roles and migrating their permissions correctly may be a pain, which
                // is actually not needed in the compatibility mode. Camera list access is limited
                // on the server side anyway, so the only major drawback here is that user can see
                // PTZ control icon even if it has no access to PTZ.
                NX_VERBOSE(this, "User has custom role. Promoting to admin as a workaround.");
                userModel.permissions = nx::vms::api::GlobalPermissionDeprecated::admin;
            }

            context->compatibilityUserModel = userModel;
        }
    }

    struct CertificateVerificationResult
    {
        bool success = false;
        bool isUserProvidedCertificate = false;
    };
    CertificateVerificationResult verifyHandshakeCertificateChain(
        const nx::network::ssl::CertificateChain& handshakeCertificateChain,
        const nx::vms::api::ServerInformation& serverInfo)
    {
        if (!nx::network::ini().verifyVmsSslCertificates)
            return {.success = true};

        std::optional<RemoteConnectionErrorCode> error;

        if (handshakeCertificateChain.empty())
            return {.success = false};

        const auto handshakeKey = handshakeCertificateChain[0].publicKey();

        if (handshakeKey == publicKey(serverInfo.userProvidedCertificatePem))
            return {.success = true, .isUserProvidedCertificate = true};

        if (handshakeKey == publicKey(serverInfo.certificatePem))
            return {.success = true};

        NX_WARNING(this,
            "The handshake certificate doesn't match any certificate provided by"
            " the server.\nHandshake key: %1",
            handshakeKey);

        if (const auto& pem = serverInfo.certificatePem; !pem.empty())
        {
            NX_VERBOSE(this,
                "Server's certificate key: %1\nServer's certificate: %2",
                publicKey(pem),
                pem);
        }

        if (const auto& pem = serverInfo.userProvidedCertificatePem; !pem.empty())
        {
            NX_VERBOSE(this,
                "User provided certificate key: %1\nServer's certificate: %2",
                publicKey(pem),
                pem);
        }

        // Handshake certificate doesn't match target server certificates.
        return {.success = false};
    }

    void processCertificates(ContextPtr context)
    {
        using CertificateType = CertificateVerifier::CertificateType;

        if (!NX_ASSERT(context))
            return;

        NX_VERBOSE(this, "Process received certificates list.");
        for (const auto& server: context->serversInfo)
        {
            const auto& serverId = server.id;
            const auto serverUrl = mainServerUrl(server.remoteAddresses, server.port);

            auto storeCertificate =
                [&](const nx::network::ssl::CertificateChain& chain, CertificateType type)
                {
                    if (chain.empty())
                        return false;

                    const auto currentKey = chain[0].publicKey();
                    const auto pinnedKey = certificateVerifier->pinnedCertificate(serverId, type);

                    if (currentKey == pinnedKey)
                        return true;

                    if (!pinnedKey)
                    {
                        // A new certificate has been found. Pin it, since the system is trusted.
                        certificateVerifier->pinCertificate(serverId, currentKey, type);
                        return true;
                    }

                    auto accept =
                        [server, serverUrl, chain]
                            (AbstractRemoteConnectionUserInteractionDelegate* delegate)
                        {
                            return delegate->acceptCertificateOfServerInTargetSystem(
                                server,
                                nx::network::SocketAddress::fromUrl(serverUrl),
                                chain);
                        };
                    if (const auto accepted = context->logonData.userInteractionAllowed
                        && executeInUiThreadSync(context, accept))
                    {
                        certificateVerifier->pinCertificate(serverId, currentKey, type);
                        return true;
                    }

                    return false;
                };

            auto processCertificate =
                [&](const std::string& pem, CertificateType type)
                {
                    if (pem.empty())
                        return true; //< There is no certificate to process.

                    const auto chain = nx::network::ssl::Certificate::parse(pem);

                    if (!storeCertificate(chain, type))
                    {
                        context->setError(RemoteConnectionErrorCode::certificateRejected);
                        return false;
                    }

                    // Certificate has been stored successfully. Add it into the cache.
                    context->certificateCache->addCertificate(
                        serverId,
                        chain[0].publicKey(),
                        type);
                    return true;
                };

            if (!processCertificate(server.certificatePem, CertificateType::autogenerated))
            {
                return;
            }

            if (!processCertificate(
                server.userProvidedCertificatePem,
                CertificateType::connection))
            {
                return;
            }
        }
    }

    void fixupCertificateCache(ContextPtr context)
    {
        if (!context)
            return;

        NX_VERBOSE(this, "Emulate certificate cache for System without REST API support.");
        if (nx::network::ini().verifyVmsSslCertificates
            && NX_ASSERT(!context->handshakeCertificateChain.empty()))
        {
            context->certificateCache->addCertificate(
                context->moduleInformation.id,
                context->handshakeCertificateChain[0].publicKey(),
                CertificateVerifier::CertificateType::autogenerated);
        }
    }

    /**
     * This method run asynchronously in a separate thread.
     */
    void connectToServerAsync(WeakContextPtr contextPtr)
    {
        auto context =
            [contextPtr]() -> ContextPtr
            {
                if (auto context = contextPtr.lock(); context && !context->failed())
                    return context;
                return {};
            };

        logInitialInfo(context());

        // Request cloud token asyncronously, as this request may go in parallel with Server api.
        // This requires to know System ID and version, so method will do nothing if we do not have
        // them yet.
        requestCloudTokenIfPossible(context());

        // If server version is not known, we should call api/moduleInformation first. Also send
        // this request for 4.2 and older systems as there is no other way to identify them.
        if (needToRequestModuleInformationFirst(context()))
        {
            // GET /api/moduleInformation
            getModuleInformation(context());

            // At this moment we definitely know System ID and version, so request token if not
            // done it yet.
            requestCloudTokenIfPossible(context());
        }

        // For Systems 5.0 and newer we may use /rest/v1/servers/*/info and receive all Servers'
        // certificates in one call. Offline Servers will not be listed if the System is 5.0, so
        // their certificates will be processed in the ServerCertificateWatcher class.
        if (systemSupportsRestApi(context()))
        {
            // GET /rest/v1/servers/*/info
            getServersInfo(context());

            // At this moment we definitely know System ID and version, so request token if not
            // done it yet.
            requestCloudTokenIfPossible(context());

            // GET /api/moduleInformation
            // Request actually will be sent for 5.0 multi-server Systems only as in 5.1 we can get
            // Server ID from the servers info list reply.
            ensureExpectedServerId(context());

            // Ensure expected Server is present in the servers info list, copy it's info to the
            // context module information field, verify handshake certificate and process all
            // certificates from the info list.
            //
            // User interaction.
            processServersInfoList(context());
        }
        else if (context()) //< 4.2 and older servers.
        {
            // User interaction.
            verifyTargetCertificate(context(), /*certificateIsUserProvided*/ false);
            fixupCertificateCache(context());
        }

        if (!checkCompatibility(context()))
            return;

        if (!isSystemCompatibleWithUser(context()))
            return;

        pinCloudConnectionAddressIfNeeded(context());
        if (!isRestApiSupported(context()))
        {
            NX_DEBUG(this, "Login with Digest to the System with no REST API support.");
            loginWithDigest(context()); //< GET /api/moduleInformationAuthenticated

            /** Emulate admin user for the old systems. */
            emulateCompatibilityUserModel(context());
            return;
        }

        if (peerType == nx::vms::api::PeerType::videowallClient)
        {
            NX_DEBUG(this, "Login as Video Wall.");
            loginWithToken(context()); //< GET /rest/v1/login/sessions/current
            return;
        }

        if (auto ctx = context(); ctx && ctx->isCloudConnection())
        {
            processCloudToken(context()); // User Interaction (2FA if needed).
            NX_DEBUG(this, "Login with Cloud Token.");
            checkServerCloudConnection(context()); //< GET /rest/v1/login/sessions/current
        }
        else if (context())
        {
            NX_DEBUG(this, "Connecting as Local User, checking whether LDAP is required.");

            // Step is performed for local Users to upgrade them to LDAP if needed - or block
            // cloud login using a Local System tile or Login Dialog.
            //
            // GET /rest/v1/login/users/<username>
            nx::vms::api::LoginUser userType = verifyLocalUserType(context());
            if (userType.methods.testFlag(nx::vms::api::LoginMethod::http))
            {
                NX_DEBUG(this, "Digest authentication is preferred for the User.");

                // Digest is the preferred method as it is the only way to use rtsp instead of
                // rtsps.
                //
                // GET /api/moduleInformationAuthenticated.
                loginWithDigest(context());
            }
            else
            {
                NX_DEBUG(this, "Logging in with a token.");

                // Try to login with an already saved token.
                if (canLoginWithSavedToken(context()))
                    loginWithToken(context()); //< GET /rest/v1/login/sessions/current
                else
                    issueLocalToken(context()); //< GET /rest/v1/login/sessions
            }
        }

        // For the older systems (before user rights redesign) expliticly request current user
        // permissions.
        if (isOldPermissionsModelUsed(context()))
        {
            // GET /rest/v1/users/<username>
            requestCompatibilityUserPermissions(context());
        }
    }
};

RemoteConnectionFactory::RemoteConnectionFactory(
    AuditIdProvider auditIdProvider,
    CloudCredentialsProvider cloudCredentialsProvider,
    std::shared_ptr<AbstractRemoteConnectionFactoryRequestsManager> requestsManager,
    CertificateVerifier* certificateVerifier,
    nx::vms::api::PeerType peerType,
    Qn::SerializationFormat serializationFormat)
    :
    AbstractECConnectionFactory(),
    d(new Private(
        this,
        peerType,
        serializationFormat,
        certificateVerifier,
        std::move(auditIdProvider),
        std::move(cloudCredentialsProvider),
        std::move(requestsManager))
    )
{
}

RemoteConnectionFactory::~RemoteConnectionFactory()
{
    shutdown();
}

void RemoteConnectionFactory::setUserInteractionDelegate(
    std::unique_ptr<AbstractRemoteConnectionUserInteractionDelegate> delegate)
{
    d->userInteractionDelegate = std::move(delegate);
}

AbstractRemoteConnectionUserInteractionDelegate*
    RemoteConnectionFactory::userInteractionDelegate() const
{
    return d->userInteractionDelegate.get();
}

void RemoteConnectionFactory::shutdown()
{
    d->requestsManager.reset();
}

RemoteConnectionFactory::ProcessPtr RemoteConnectionFactory::connect(
    LogonData logonData,
    Callback callback,
    std::unique_ptr<AbstractRemoteConnectionUserInteractionDelegate> customUserInteractionDelegate)
{
    auto process = std::make_shared<RemoteConnectionProcess>();

    process->context->logonData = logonData;
    process->context->customUserInteractionDelegate = std::move(customUserInteractionDelegate);
    process->context->certificateCache = std::make_shared<CertificateCache>();

    process->future = std::async(std::launch::async,
        [this, contextPtr = WeakContextPtr(process->context), callback]
        {
            nx::utils::setCurrentThreadName("RemoteConnectionFactoryThread");

            d->connectToServerAsync(contextPtr);

            if (!contextPtr.lock())
                return;

            QMetaObject::invokeMethod(
                this,
                [this, contextPtr, callback]()
                {
                    auto context = contextPtr.lock();
                    if (!context)
                        return;

                    if (context->error())
                        callback(*context->error());
                    else
                        callback(d->makeRemoteConnectionInstance(context));
                },
                Qt::QueuedConnection);
        });
    return process;
}

void RemoteConnectionFactory::destroyAsync(ProcessPtr&& process)
{
    NX_ASSERT(process.use_count() == 1);
    std::thread([process = std::move(process)]{ }).detach();
}

} // namespace nx::vms::client::core
