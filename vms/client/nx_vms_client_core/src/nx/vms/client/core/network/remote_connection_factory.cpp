// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_factory.h"
#include "private/remote_connection_factory_requests.h"

#include <memory>

#include <QtCore/QPointer>

#include <common/common_module.h>
#include <common/common_module_aware.h>
#include <network/system_helpers.h>
#include <nx/network/address_resolver.h>
#include <nx/network/nx_network_ini.h>
#include <nx/network/socket_global.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/thread/thread_util.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx_ec/ec_api_fwd.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>
#include <utils/email/email.h>
#include <watchers/cloud_status_watcher.h>

#include "certificate_verifier.h"
#include "cloud_connection_factory.h"
#include "network_manager.h"
#include "remote_connection.h"
#include "remote_connection_user_interaction_delegate.h"

using namespace ec2;
using ServerCompatibilityValidator = nx::vms::common::ServerCompatibilityValidator;

namespace nx::vms::client::core {

namespace {

static const nx::vms::api::SoftwareVersion kRestApiSupportVersion(5, 0);

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

std::optional<std::string> publicKey(const std::optional<std::string>& pem)
{
    if (!pem)
        return {};

    auto chain = nx::network::ssl::Certificate::parse(*pem);
    if (chain.empty())
        return {};

    return chain[0].publicKey();
};

} // namespace

using WeakContextPtr = std::weak_ptr<RemoteConnectionFactory::Context>;

struct RemoteConnectionFactory::Private: public /*mixin*/ QnCommonModuleAware
{
    using RequestsManager = detail::RemoteConnectionFactoryRequestsManager;

    ec2::AbstractECConnectionFactory* q;
    const nx::vms::api::PeerType peerType;
    const Qn::SerializationFormat serializationFormat;
    QPointer<CertificateVerifier> certificateVerifier;
    std::unique_ptr<AbstractRemoteConnectionUserInteractionDelegate> userInteractionDelegate;
    std::unique_ptr<CloudConnectionFactory> cloudConnectionFactory;

    std::unique_ptr<RequestsManager> requestsManager;

    Private(
        ec2::AbstractECConnectionFactory* q,
        nx::vms::api::PeerType peerType,
        Qn::SerializationFormat serializationFormat,
        CertificateVerifier* certificateVerifier,
        QnCommonModule* commonModule)
        :
        QnCommonModuleAware(commonModule),
        q(q),
        peerType(peerType),
        serializationFormat(serializationFormat),
        certificateVerifier(certificateVerifier)
    {
        requestsManager = std::make_unique<detail::RemoteConnectionFactoryRequestsManager>(
            certificateVerifier);

        cloudConnectionFactory = std::make_unique<CloudConnectionFactory>();
    }

    RemoteConnectionPtr makeRemoteConnectionInstance(ContextPtr context)
    {
        ConnectionInfo connectionInfo(context->info);
        connectionInfo.userInteractionAllowed = true;

        return std::make_shared<RemoteConnection>(
            peerType,
            context->moduleInformation,
            connectionInfo,
            context->sessionTokenExpirationTime,
            certificateVerifier,
            context->certificateCache,
            serializationFormat,
            /*auditId*/ commonModule()->runningInstanceGUID());
    }

    bool executeInUiThreadSync(std::function<bool()> handler)
    {
        std::promise<bool> isAccepted;
        executeInThread(userInteractionDelegate->thread(),
            [&isAccepted, handler]()
            {
                isAccepted.set_value(handler());
            });
        return isAccepted.get_future().get();
    }

    void fillModuleInformationAndCertificate(ContextPtr context)
    {
        if (context)
            requestsManager->fillModuleInformationAndCertificate(context);
    }

    void checkCompatibility(ContextPtr context)
    {
        if (context)
        {
            if (context->moduleInformation.isNewSystem())
            {
                context->error = RemoteConnectionErrorCode::factoryServer;
            }
            else if (const auto incompatibilityReason = ServerCompatibilityValidator::check(
                context->moduleInformation))
            {
                context->error = toErrorCode(*incompatibilityReason);
            }
        }
    }

    void verifyExpectedServerId(ContextPtr context, std::optional<QnUuid> expectedServerId)
    {
        if (context && expectedServerId && *expectedServerId != context->moduleInformation.id)
            context->error = RemoteConnectionErrorCode::unexpectedServerId;
    }

    void checkServerCertificateEquality(ContextPtr context)
    {
        if (!context || !nx::network::ini().verifyVmsSslCertificates)
            return;

        const auto provided = requestsManager->targetServerCertificates(context);
        if (NX_ASSERT(!context->certificateChain.empty()))
        {
            const auto handshakeKey = context->certificateChain[0].publicKey();

            if (handshakeKey == publicKey(provided.userProvidedCertificate))
            {
                context->targetHasUserProvidedCertificate = true;
                return;
            }

            if (handshakeKey == publicKey(provided.certificate))
                return;
        }

        // Handshake certificate doesn't match target server certificates.
        context->error = RemoteConnectionErrorCode::certificateRejected;
    }

    void verifyCertificate(ContextPtr context)
    {
        if (!context || !nx::network::ini().verifyVmsSslCertificates)
            return;

        // Make sure factory is setup correctly.
        if (!NX_ASSERT(certificateVerifier) || !NX_ASSERT(userInteractionDelegate))
        {
            context->error = RemoteConnectionErrorCode::internalError;
            return;
        }

        const CertificateVerifier::Status status = certificateVerifier->verifyCertificate(
            context->moduleInformation.id,
            context->certificateChain);

        if (status == CertificateVerifier::Status::ok)
            return;

        auto pinTargetServerCertificate =
            [this, context]
            {
                if (!NX_ASSERT(!context->certificateChain.empty()))
                    return;

                // Server certificates are checked in checkServerCertificateEquality() using
                // REST API. After that we know exactly which type of certificate is used on
                // SSL handshake. For old servers that don't support REST API the certificate
                // is stored as auto-generated.
                certificateVerifier->pinCertificate(
                    context->moduleInformation.id,
                    context->certificateChain[0].publicKey(),
                    context->targetHasUserProvidedCertificate
                        ? CertificateVerifier::CertificateType::connection
                        : CertificateVerifier::CertificateType::autogenerated);
            };

        const auto serverName = context->info.address.address.toString();
        NX_VERBOSE(
            this,
            "New certificate has been received from %1. "
            "Trying to verify it by system CA certificates.",
            serverName);

        std::string errorMessage;
        if (NX_ASSERT(!context->certificateChain.empty())
            && nx::network::ssl::verifyBySystemCertificates(
                context->certificateChain, serverName, &errorMessage))
        {
            NX_VERBOSE(this, "Certificate verification for %1 is successful.", serverName);
            pinTargetServerCertificate();
            return;
        }

        NX_VERBOSE(this, errorMessage);

        auto accept =
            [this, status, context]
            {
                if (status == CertificateVerifier::Status::notFound)
                {
                    return userInteractionDelegate->acceptNewCertificate(
                        context->moduleInformation,
                        context->info.address,
                        context->certificateChain);
                }
                else if (status == CertificateVerifier::Status::mismatch)
                {
                    return userInteractionDelegate->acceptCertificateAfterMismatch(
                        context->moduleInformation,
                        context->info.address,
                        context->certificateChain);
                }
                return false;
            };

        if (auto accepted = context->info.userInteractionAllowed
            && executeInUiThreadSync(accept))
        {
            pinTargetServerCertificate();
        }
        else
        {
            // User rejected the certificate.
            context->error = RemoteConnectionErrorCode::certificateRejected;
        }
    }

    // For cloud connections we can get url, containing only system id.
    // In that case each request may potentially be sent to another server, which may lead
    // to undefined behavior. So we are replacing generic url with the exact server's one.
    // Server address should be fixed as soon as possible.
    void fixCloudConnectionInfoIfNeeded(ContextPtr context)
    {
        if (!context)
            return;

        const std::string hostname = context->info.address.address.toString();
        if (nx::network::SocketGlobals::addressResolver().isCloudHostname(hostname))
        {
            const auto fullHostname = context->moduleInformation.id.toSimpleString()
                + '.'
                + context->moduleInformation.cloudSystemId;
            context->info.address.address = fullHostname;
            NX_DEBUG(this, "Fixed connection address: %1", fullHostname);
        }

        if (context->info.userType == nx::vms::api::UserType::cloud)
        {
            auto& credentials = context->info.credentials;

            // Use refresh token to issue new session token if server supports OAuth cloud
            // authentication through the REST API.
            if (isRestApiSupported(context))
            {
                credentials = qnCloudStatusWatcher->remoteConnectionCredentials();
            }
            // Current or stored credentials will be passed to compatibility mode client.
        }
    }

    void checkServerCloudConnection(ContextPtr context)
    {
        if (!context)
            return;

        requestsManager->getCurrentSession(context);

        // Assuming server cloud connection problems if fresh cloud token is unauthorized
        if (context->failed() && context->error == RemoteConnectionErrorCode::sessionExpired)
            context->error = RemoteConnectionErrorCode::cloudUnavailableOnServer;
    }

    bool isRestApiSupported(ContextPtr context)
    {
        return context && context->moduleInformation.version >= kRestApiSupportVersion;
    }

    bool isSystemCompatibleWithUser(ContextPtr context)
    {
        // The system version below 5.0 is not compatible with a cloud user with 2fa enabled
        if (context && !isRestApiSupported(context)
            && context->info.userType == nx::vms::api::UserType::cloud
            && qnCloudStatusWatcher->is2FaEnabledForUser())
        {
            context->error = RemoteConnectionErrorCode::systemIsNotCompatibleWith2Fa;
            return false;
        }

        return true;
    }

    void loginWithDigest(ContextPtr context)
    {
        if (context)
        {
            ensureUserNameIsLowercaseIfDigest(context->info.credentials);
            requestsManager->checkDigestAuthentication(context);
        }
    }

    bool loginWithToken(ContextPtr context)
    {
        if (!context)
            return false;

        if (context->info.credentials.authToken.isBearerToken())
        {
            // In case token is outdated we will receive unauthorized here.
            nx::vms::api::LoginSession currentSession = requestsManager->getCurrentSession(context);
            if (!context->failed())
            {
                context->info.credentials.username = currentSession.username.toStdString();
                context->sessionTokenExpirationTime =
                    qnSyncTime->currentTimePoint() + currentSession.expiresInS;
            }
            return true;
        }
        return false;
    }

    nx::vms::api::LoginUser verifyUserType(ContextPtr context)
    {
        if (context)
        {
            nx::vms::api::LoginUser loginUserData = requestsManager->getUserType(context);
            if (context->failed())
                return {};

            // Check if expected user type does not match actual. Possible scenarios:
            // * Receive cloud user using the local tile - forbidden, throw error.
            // * Receive ldap user using the local tile - OK, updating actual user type.
            // * Other cases such as receive local user using cloud tile - internal error.
            if (context->info.userType != loginUserData.type)
            {
                if (!NX_ASSERT(context->info.userType == nx::vms::api::UserType::local))
                    context->error = RemoteConnectionErrorCode::internalError;
                else if (loginUserData.type == nx::vms::api::UserType::cloud)
                    context->error = RemoteConnectionErrorCode::loginAsCloudUserForbidden;
                else if (NX_ASSERT(loginUserData.type == nx::vms::api::UserType::ldap))
                    context->info.userType = loginUserData.type;
            }
            return loginUserData;
        }
        return {};
    }

    void issueCloudToken(ContextPtr context)
    {
        if (!context)
            return;

        auto cloudConnection = cloudConnectionFactory->createConnection();
        nx::cloud::db::api::IssueTokenResponse response = requestsManager->issueCloudToken(
            context, cloudConnection.get());

        if (!context->failed())
        {
            NX_DEBUG(this, "Token response error: %1", response.error);
            if (response.error)
            {
                if (*response.error == nx::cloud::db::api::OauthManager::k2faRequiredError)
                {
                    auto validate =
                        [this, token = response.access_token]
                        {
                            return userInteractionDelegate->request2FaValidation(token);
                        };

                    const bool validated = context->info.userInteractionAllowed
                        && executeInUiThreadSync(validate);

                    if (!validated)
                        context->error = RemoteConnectionErrorCode::unauthorized;
                }
                else if (*response.error
                    == nx::cloud::db::api::OauthManager::k2faDisabledForUserError)
                {
                    context->error = RemoteConnectionErrorCode::twoFactorAuthOfCloudUserIsDisabled;
                }
                else
                {
                    context->error = RemoteConnectionErrorCode::unauthorized;
                }
            }
        }

        if (!context->failed())
        {
            context->info.credentials.authToken =
                nx::network::http::BearerAuthToken(response.access_token);
            context->sessionTokenExpirationTime = response.expires_at;
        }
    }

    void issueLocalToken(ContextPtr context)
    {
        if (!context)
            return;

        nx::vms::api::LoginSession session = requestsManager->createLocalSession(context);
        if (!context->failed())
        {
            context->info.credentials.username = session.username.toStdString();
            context->info.credentials.authToken = nx::network::http::BearerAuthToken(session.token);
            context->sessionTokenExpirationTime =
                qnSyncTime->currentTimePoint() + session.expiresInS;
        }
    }

    void pullRestCertificates(ContextPtr context)
    {
        using CertificateType = CertificateVerifier::CertificateType;

        if (!context)
            return;

        context->certificateCache = std::make_shared<CertificateCache>();

        const auto certificatesInfo = requestsManager->pullRestCertificates(context);
        for (const auto& info: certificatesInfo)
        {
            const auto& serverId = info.serverId;
            const auto& serverInfo = info.serverInfo;
            const auto& serverUrl = info.serverUrl;

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
                        [this, serverInfo, serverUrl, chain]
                        {
                            return userInteractionDelegate->acceptCertificateOfServerInTargetSystem(
                                serverInfo,
                                nx::network::SocketAddress::fromUrl(serverUrl),
                                chain);
                        };
                    if (const auto accepted = context->info.userInteractionAllowed
                        && executeInUiThreadSync(accept))
                    {
                        certificateVerifier->pinCertificate(serverId, currentKey, type);
                        return true;
                    }

                    return false;
                };

            auto processCertificate =
                [&](const std::optional<std::string>& pem, CertificateType type)
                {
                    if (!pem)
                        return true; //< There is no certificate to process.

                    const auto chain = nx::network::ssl::Certificate::parse(*pem);

                    if (!storeCertificate(chain, type))
                    {
                        context->error = RemoteConnectionErrorCode::certificateRejected;
                        return false;
                    }

                    // Certificate has been stored successfully. Add it into the cache.
                    context->certificateCache->addCertificate(serverId, chain[0].publicKey(), type);
                    return true;
                };

            if (!processCertificate(info.certificate, CertificateType::autogenerated))
                return;
            if (!processCertificate(info.userProvidedCertificate, CertificateType::connection))
                return;
        }
    }

    void fixupCertificateCache(ContextPtr context)
    {
        if (!context)
            return;

        context->certificateCache = std::make_shared<CertificateCache>();
        if (NX_ASSERT(!context->certificateChain.empty()))
        {
            context->certificateCache->addCertificate(
                context->moduleInformation.id,
                context->certificateChain[0].publicKey(),
                CertificateVerifier::CertificateType::autogenerated);
        }
    }

    /**
     * This method run asynchronously in a separate thread.
     */
    void connectToServerAsync(
        WeakContextPtr contextPtr,
        std::optional<QnUuid> expectedServerId)
    {
        auto context =
            [contextPtr]() -> ContextPtr
            {
                if (auto context = contextPtr.lock(); context && !context->failed())
                    return context;
                return {};
            };

        fillModuleInformationAndCertificate(context());
        verifyExpectedServerId(context(), expectedServerId);
        fixCloudConnectionInfoIfNeeded(context());
        if (isRestApiSupported(context()))
            checkServerCertificateEquality(context());
        verifyCertificate(context());
        checkCompatibility(context());

        if (!isSystemCompatibleWithUser(context()))
            return;

        if (!isRestApiSupported(context()))
        {
            loginWithDigest(context());
            fixupCertificateCache(context());
            return;
        }

        if (peerType == nx::vms::api::PeerType::videowallClient)
        {
            loginWithToken(context());
            pullRestCertificates(context());
            return;
        }

        nx::vms::api::LoginUser userType = verifyUserType(context());

        if (userType.type == nx::vms::api::UserType::cloud)
        {
            issueCloudToken(context());
            checkServerCloudConnection(context());
        }
        else if (userType.methods.testFlag(nx::vms::api::LoginMethod::http))
        {
            // Digest is the preferred method as it is the only way to use rtsp instead of rtsps.
            loginWithDigest(context());
        }
        else
        {
            // Try to login with an already saved token if present.
            if (!loginWithToken(context()))
                issueLocalToken(context());
        }

        pullRestCertificates(context());
    }
};

RemoteConnectionFactory::RemoteConnectionFactory(
    QnCommonModule* commonModule,
    CertificateVerifier* certificateVerifier,
    nx::vms::api::PeerType peerType,
    Qn::SerializationFormat serializationFormat)
    :
    AbstractECConnectionFactory(),
    d(new Private(this, peerType, serializationFormat, certificateVerifier, commonModule))
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

void RemoteConnectionFactory::shutdown()
{
    d->requestsManager.reset();
}

RemoteConnectionFactory::ProcessPtr RemoteConnectionFactory::connect(
    ConnectionInfo connectionInfo,
    std::optional<QnUuid> expectedServerId,
    Callback callback)
{
    auto process = std::make_shared<RemoteConnectionProcess>();

    process->context->info = connectionInfo;

    process->future = std::async(std::launch::async,
        [this, contextPtr = WeakContextPtr(process->context), expectedServerId, callback]
        {
            nx::utils::setCurrentThreadName("RemoteConnectionFactoryThread");

            d->connectToServerAsync(contextPtr, expectedServerId);

            if (!contextPtr.lock())
                return;

            QMetaObject::invokeMethod(
                this,
                [this, contextPtr, callback]()
                {
                    auto context = contextPtr.lock();
                    if (!context)
                        return;

                    if (context->error)
                        callback(*context->error);
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
