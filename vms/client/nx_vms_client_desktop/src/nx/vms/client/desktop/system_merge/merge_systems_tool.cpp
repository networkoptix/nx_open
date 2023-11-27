// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "merge_systems_tool.h"

#include <core/resource/media_server_resource.h>
#include <network/system_helpers.h>
#include <nx/branding.h>
#include <nx/network/http/http_client.h>
#include <nx/network/nx_network_ini.h>
#include <nx/network/ssl/certificate.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/core/network/remote_connection_user_interaction_delegate.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/license/remote_licenses.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace {

MergeSystemsStatus mergeStatusFromResult(const nx::network::rest::Result& result)
{
    using namespace nx::network::rest;

    switch (result.error)
    {
        case Result::NoError:
            return MergeSystemsStatus::ok;
        case Result::Forbidden:
            return MergeSystemsStatus::forbidden;
        case Result::NotFound:
            return MergeSystemsStatus::notFound;
        case Result::Unauthorized:
            return MergeSystemsStatus::unauthorized;
        default:
            return MergeSystemsStatus::unknownError;
    }
}

nx::network::ssl::CertificateChain parseCertificateChainSafe(const std::string& pemString)
{
    return pemString.empty()
        ? nx::network::ssl::CertificateChain()
        : nx::network::ssl::Certificate::parse(pemString);
}

} // namespace

namespace nx::vms::client::desktop {

struct MergeSystemsTool::Context
{
    QnUuid id; //< Merge context id, can be used for merge after ping.
    QnMediaServerResourcePtr proxy; //< Proxy for target requests.
    nx::network::SocketAddress target; //< Target server address.
    std::string targetUser;
    std::string targetPassword;
    nx::vms::api::ServerInformation targetInfo;
    nx::vms::api::SystemMergeData mergeData;
    nx::network::ssl::CertificateChain targetHandshakeChain;
    QnLicenseList targetLicenses;

    std::string targetPublicKey() const
    {
        return NX_ASSERT(!targetHandshakeChain.empty()) ? targetHandshakeChain[0].publicKey() : "";
    }
};

MergeSystemsTool::MergeSystemsTool(
    QObject* parent,
    CertificateVerifier* certificateVerifier,
    Delegate* delegate,
    const std::string& locale)
    :
    QObject(parent),
    m_requestManager(std::make_unique<MergeSystemRequestsManager>(this, locale)),
    m_certificateVerifier(certificateVerifier),
    m_delegate(delegate)
{
    NX_ASSERT(certificateVerifier);
    NX_ASSERT(delegate);
}

MergeSystemsTool::~MergeSystemsTool()
{
}

QnUuid MergeSystemsTool::pingSystem(
    const QnMediaServerResourcePtr& proxy,
    const nx::utils::Url& targetUrl,
    const nx::network::http::Credentials& targetCredentials,
    std::optional<DryRunSettings> dryRunSettings)
{
    auto ctxId = QnUuid::createUuid();

    Context ctx;
    ctx.id = ctxId;
    ctx.proxy = proxy;
    ctx.target = nx::network::SocketAddress::fromUrl(targetUrl);
    ctx.targetUser = targetCredentials.username;
    ctx.targetPassword = targetCredentials.authToken.value;

    if (dryRunSettings)
    {
        ctx.mergeData.takeRemoteSettings = !dryRunSettings->ownSettings;
        ctx.mergeData.mergeOneServer = dryRunSettings->oneServer;
        ctx.mergeData.ignoreIncompatible = dryRunSettings->ignoreIncompatible;
        ctx.mergeData.dryRun = true;
    }

    NX_DEBUG(this, "Starting ping server at address %1", ctx.target);

    auto onTargetInfo =
        [this, ctxId](rest::ErrorOrData<nx::vms::api::ServerInformation> errorOrData)
        {
            if (auto ctx = findContext(ctxId))
                at_serverInfoReceived(*ctx, errorOrData);
        };

    auto certificateSaveFunc =
        [this, ctxId](
            const nx::network::ssl::CertificateChainView& chain)
        {
            if (auto ctx = findContext(ctxId); ctx && !chain.empty())
            {
                ctx->targetHandshakeChain.clear();
                for (const auto& view: chain)
                    ctx->targetHandshakeChain.emplace_back(view);
                return true;
            }
            return false;
        };

    m_requestManager->getTargetInfo(
        ctx.proxy,
        m_certificateVerifier->makeAdapterFunc(ctx.proxy->getId(), ctx.proxy->getApiUrl()),
        ctx.target,
        nx::network::ssl::makeAdapterFunc(std::move(certificateSaveFunc)),
        nx::utils::guarded(this, std::move(onTargetInfo)));
    m_contextMap.emplace(ctxId, ctx);
    return ctxId;
}

void MergeSystemsTool::mergeSystemDryRun(const Context& ctx)
{
    NX_DEBUG(this, "Starting system merge dry run, target: %1", ctx.target);

    auto mergeData = ctx.mergeData;
    mergeData.ignoreOfflineServerDuplicates = true;
    mergeData.remoteEndpoint = ctx.target.toString();
    // FIXME: Use handshake certificate here when CertificateChain PEM serialization
    // will be available.
    mergeData.remoteCertificatePem =
        ctx.targetInfo.userProvidedCertificatePem.empty()
        ? ctx.targetInfo.certificatePem
        : ctx.targetInfo.userProvidedCertificatePem;

    NX_ASSERT(mergeData.dryRun);
    mergeData.dryRun = true;

    auto onMergeDryRunStarted =
        [this, ctxId = ctx.id](
            const rest::ErrorOrData<nx::vms::api::MergeStatusReply>& errorOrData)
        {
            if (auto ctx = findContext(ctxId))
            {
                if (const auto status = std::get_if<nx::vms::api::MergeStatusReply>(&errorOrData))
                {
                    reportSystemFound(
                        *ctx,
                        MergeSystemsStatus::ok,
                        /*errorText*/ {},
                        /*removeCtx*/ false,
                        *status);
                }
                else if (const auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
                {
                    NX_WARNING(
                        this, "Can't dry run merge, rest result: %1", QJson::serialized(*error));

                    reportSystemFound(
                        *ctx, mergeStatusFromResult(*error), error->errorString, true,
                        nx::vms::api::MergeStatusReply());
                }
            }
        };

    const auto context = desktop::SystemContext::fromResource(ctx.proxy);
    nx::network::http::Credentials credentials;
    if (NX_ASSERT(context))
        credentials = context->connectionCredentials();

    m_requestManager->mergeSystem(
        ctx.proxy,
        m_certificateVerifier->makeAdapterFunc(ctx.proxy->getId(), ctx.proxy->getApiUrl()),
        credentials,
        mergeData,
        nx::utils::guarded(this, std::move(onMergeDryRunStarted)));
}

bool MergeSystemsTool::mergeSystem(
    const QnUuid& ctxId,
    const std::string& ownerSessionToken,
    bool ownSettings,
    bool oneServer,
    bool ignoreIncompatible)
{
    Context* ctx = findContext(ctxId);
    if (!ctx)
        return false;

    NX_DEBUG(this, "Starting system merge, target: %1", ctx->target);
    ctx->mergeData.takeRemoteSettings = !ownSettings;
    ctx->mergeData.mergeOneServer = oneServer;
    ctx->mergeData.ignoreIncompatible = ignoreIncompatible;
    ctx->mergeData.ignoreOfflineServerDuplicates = true;
    ctx->mergeData.remoteEndpoint = ctx->target.toString();
    // FIXME: Use handshake certificate here when CertificateChain PEM serialization
    // will be available.
    ctx->mergeData.remoteCertificatePem =
        ctx->targetInfo.userProvidedCertificatePem.empty()
        ? ctx->targetInfo.certificatePem
        : ctx->targetInfo.userProvidedCertificatePem;

    ctx->mergeData.dryRun = false;

    auto onMergeStarted =
        [this, ctxId](const rest::ErrorOrData<nx::vms::api::MergeStatusReply>& errorOrData)
        {
            if (auto ctx = findContext(ctxId))
                at_mergeStarted(*ctx, errorOrData);
        };

    m_requestManager->mergeSystem(
        ctx->proxy,
        m_certificateVerifier->makeAdapterFunc(ctx->proxy->getId(), ctx->proxy->getApiUrl()),
        nx::network::http::BearerAuthToken(ownerSessionToken),
        ctx->mergeData,
        nx::utils::guarded(this, std::move(onMergeStarted)));

    return true;
}

MergeSystemsTool::Context* MergeSystemsTool::findContext(const QnUuid& target)
{
    auto it = m_contextMap.find(target);
    return (it != m_contextMap.end()) ? &it->second : nullptr;
}

void MergeSystemsTool::reportMergeFinished(
    const Context& ctx,
    MergeSystemsStatus status,
    const QString& errorText)
{
    NX_DEBUG(this, "Merge finished, target: %1, status: %2, error: %3",
        ctx.target, status, errorText);

    auto targetInfo = ctx.targetInfo;
    m_contextMap.erase(ctx.mergeData.remoteServerId);

    emit mergeFinished(status, errorText, targetInfo);
}

void MergeSystemsTool::reportSystemFound(
    const Context& ctx,
    MergeSystemsStatus status,
    const QString& errorText,
    bool removeCtx,
    const nx::vms::api::MergeStatusReply& reply)
{
    NX_DEBUG(this, "System found, target: %1, status: %2, error: %3",
        ctx.target, status, errorText);

    auto targetInfo = ctx.targetInfo;

    if (removeCtx)
        m_contextMap.erase(ctx.mergeData.remoteServerId);

    emit systemFound(status, errorText, targetInfo, reply);
}

void MergeSystemsTool::at_serverInfoReceived(
    Context& ctx,
    const rest::ErrorOrData<nx::vms::api::ServerInformation>& errorOrData)
{
    if (auto serverInfo = std::get_if<nx::vms::api::ServerInformation>(&errorOrData))
    {
        ctx.targetInfo = std::move(*serverInfo);

        if (auto status = verifyTargetCertificate(ctx); status != MergeSystemsStatus::ok)
        {
            NX_VERBOSE(this, "Server certificate verificaton status: %1", status);
            reportSystemFound(ctx, status, {}, true);

            return;
        }

        auto onSessionCreated =
            [this, ctxId = ctx.id](
                rest::ErrorOrData<nx::vms::api::LoginSession> errorOrData)
            {
                if (auto ctx = findContext(ctxId))
                    at_sessionCreated(*ctx, errorOrData);
            };

        nx::vms::api::LoginSessionRequest request;
        request.username = QString::fromStdString(ctx.targetUser);
        request.password = QString::fromStdString(ctx.targetPassword);

        m_requestManager->createTargetSession(
            ctx.proxy,
            m_certificateVerifier->makeAdapterFunc(ctx.proxy->getId(), ctx.proxy->getApiUrl()),
            ctx.target,
            m_certificateVerifier->makeRestrictedAdapterFunc(ctx.targetPublicKey()),
            request,
            nx::utils::guarded(this, std::move(onSessionCreated)));
    }
    else if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
    {
        NX_WARNING(this, "Can't get server info, rest result: %1", QJson::serialized(*error));
        // Older versions responds with {"error" : 404 } special processing is required.
        if ((static_cast<nx::network::http::StatusCode::Value>(error->error)
            == nx::network::http::StatusCode::notFound)
            || (error->error == nx::network::rest::Result::NotFound))
        {
            reportSystemFound(ctx, MergeSystemsStatus::incompatibleInternal, QString(), true);
        }
        else
        {
            reportSystemFound(
                ctx, mergeStatusFromResult(*error), error->errorString, true);
        }
    }
}

void MergeSystemsTool::at_sessionCreated(
    Context& ctx,
    const rest::ErrorOrData<nx::vms::api::LoginSession>& errorOrData)
{
    if (auto loginSession = std::get_if<nx::vms::api::LoginSession>(&errorOrData))
    {
        NX_ASSERT(!loginSession->token.empty());
        ctx.mergeData.remoteSessionToken = std::move(loginSession->token);

        if (ctx.mergeData.dryRun)
        {
            // Skip further checks and do a dry run - the server should perform all other checks.
            mergeSystemDryRun(ctx);
            return;
        }

        if (!nx::vms::license::hasUniqueLicenses(ctx.proxy->systemContext()->licensePool()))
        {
            NX_DEBUG(this, "Unique license check failed");
            reportSystemFound(ctx, MergeSystemsStatus::ok, {}, false);
            return;
        }

        auto onLicenseReceived =
            [this, ctxId = ctx.id](
                const rest::ErrorOrData<nx::vms::api::LicenseDataList>& errorOrData)
        {
            if (auto ctx = findContext(ctxId))
                at_licensesReceived(*ctx, errorOrData);
        };

        m_requestManager->getTargetLicenses(
            ctx.proxy,
            m_certificateVerifier->makeAdapterFunc(ctx.proxy->getId(), ctx.proxy->getApiUrl()),
            ctx.target,
            ctx.mergeData.remoteSessionToken,
            m_certificateVerifier->makeRestrictedAdapterFunc(ctx.targetPublicKey()),
            std::move(onLicenseReceived)
        );

        return;
    }
    else if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
    {
        NX_WARNING(this, "Can't create session, rest result: %1", QJson::serialized(*error));
        reportSystemFound(ctx, mergeStatusFromResult(*error), error->errorString, true);
    }
}

void MergeSystemsTool::at_licensesReceived(
    Context& ctx,
    const rest::ErrorOrData<nx::vms::api::LicenseDataList>& errorOrData)
{
    if (auto licenseDataList = std::get_if<nx::vms::api::LicenseDataList>(&errorOrData))
    {
        ec2::fromApiToResourceList(*licenseDataList, ctx.targetLicenses);
        auto status = nx::vms::license::remoteLicensesConflict(
            ctx.proxy->systemContext()->licensePool(),
            ctx.targetLicenses);

        NX_VERBOSE(this, "License conflict check result: %1", status);
        reportSystemFound(ctx, status, {}, false);
    }
    else if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
    {
        NX_WARNING(this, "Can't get licenses, rest result: %1", QJson::serialized(*error));
        reportSystemFound(ctx, mergeStatusFromResult(*error), error->errorString, true);
    }
}

void MergeSystemsTool::at_mergeStarted(
    Context& ctx,
    const rest::ErrorOrData<nx::vms::api::MergeStatusReply>& errorOrData)
{
    if (auto mergeStatus = std::get_if<nx::vms::api::MergeStatusReply>(&errorOrData))
    {
        NX_VERBOSE(this, "Merge status received, id: %1, in progress: %2",
            mergeStatus->mergeId, mergeStatus->mergeInProgress);

        if (mergeStatus->mergeInProgress)
            reportMergeFinished(ctx, MergeSystemsStatus::ok);
        else
            reportMergeFinished(ctx, MergeSystemsStatus::unknownError);
    }
    else if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
    {
        NX_WARNING(this, "Can't merge systems, rest result: %1", QJson::serialized(*error));
        reportMergeFinished(ctx, mergeStatusFromResult(*error), error->errorString);
    }
}

bool MergeSystemsTool::checkServerCertificateEquality(const Context& ctx)
{
    auto chainInfo =
        [](const QString& name, const nx::network::ssl::CertificateChain& chain)
        {
            QString serial;
            if (!chain.empty())
                serial.setNum(chain[0].serialNumber(), 16);

            return nx::format("%1 chain length: %2, serial %3", name, chain.size(), serial);
        };

    auto serverChain = parseCertificateChainSafe(ctx.targetInfo.certificatePem);
    auto userProvidedChain = parseCertificateChainSafe(ctx.targetInfo.userProvidedCertificatePem);

    NX_VERBOSE(NX_SCOPE_TAG, chainInfo("Handshake", ctx.targetHandshakeChain));
    NX_VERBOSE(NX_SCOPE_TAG, chainInfo("Server", serverChain));
    NX_VERBOSE(NX_SCOPE_TAG, chainInfo("User provided", userProvidedChain));

    if (ctx.targetHandshakeChain.empty() || serverChain.empty())
        return false;

    return (serverChain == ctx.targetHandshakeChain)
        || (userProvidedChain == ctx.targetHandshakeChain);
}

MergeSystemsStatus MergeSystemsTool::verifyTargetCertificate(const Context& ctx)
{
    if (!nx::network::ini().verifyVmsSslCertificates)
        return MergeSystemsStatus::ok;

    const auto serverName = ctx.target.address.toString();
    std::string errorMessage;

    NX_VERBOSE(this, "Try to verify target cert from %1 by system CA certificates.", serverName);
    if (nx::network::ssl::verifyBySystemCertificates(
            ctx.targetHandshakeChain, serverName, &errorMessage))
    {
        NX_VERBOSE(this, "Certificate verification for %1 is successful.", serverName);
        m_certificateVerifier->pinCertificate(
            ctx.targetInfo.id,
            ctx.targetHandshakeChain[0].publicKey());

        // Trust the certificate if it passed system storage check.
        return MergeSystemsStatus::ok;
    }

    NX_VERBOSE(this, "System CA verification result: %1", errorMessage);

    if (!checkServerCertificateEquality(ctx))
        return MergeSystemsStatus::certificateInvalid;

    // At this point target handshake chain equals user provided chain.

    const CertificateVerifier::Status status = m_certificateVerifier->verifyCertificate(
        ctx.targetInfo.id,
        ctx.targetHandshakeChain);

    if (status == CertificateVerifier::Status::ok)
        return MergeSystemsStatus::ok;

    bool accepted = false;
    if (status == CertificateVerifier::Status::notFound)
    {
        accepted = m_delegate->acceptNewCertificate(
            ctx.targetInfo, ctx.target, ctx.targetHandshakeChain);
    }
    else if (status == CertificateVerifier::Status::mismatch)
    {
        accepted = m_delegate->acceptCertificateAfterMismatch(
            ctx.targetInfo, ctx.target, ctx.targetHandshakeChain);
    }

    if (!accepted)
        return MergeSystemsStatus::certificateRejected;

    m_certificateVerifier->pinCertificate(
            ctx.targetInfo.id, ctx.targetHandshakeChain[0].publicKey());
    return MergeSystemsStatus::ok;
}

QString MergeSystemsTool::getErrorMessage(
    MergeSystemsStatus value,
    const nx::vms::api::ModuleInformation& moduleInformation)
{
    const auto systemName = helpers::isNewSystem(moduleInformation)
            ? '"' + tr("New System") + '"'
            : moduleInformation.systemName;

    switch (value)
    {
        case MergeSystemsStatus::ok:
            return QString();
        case MergeSystemsStatus::notFound:
            return tr("System was not found.");
        case MergeSystemsStatus::incompatibleVersion:
            return tr("The discovered System %1 has an incompatible version %2.",
                "%1 is name of System, %2 is version information")
                .arg(systemName).arg(moduleInformation.version.toString());
        case MergeSystemsStatus::incompatibleInternal:
            // System info is absent in this case.
            return tr("Selected System has an older software version that is incompatible with"
                " the current System. Update selected System to the latest build to merge it with"
                " the current one.");
        case MergeSystemsStatus::unauthorized:
            return tr("The password or user name is invalid.");
        case MergeSystemsStatus::forbidden:
            return tr("This user does not have permissions for the requested operation.");
        case MergeSystemsStatus::backupFailed:
            return tr("Cannot create database backup.");
        case MergeSystemsStatus::starterLicense:
            return tr("You are about to merge Systems with Starter licenses.")
                + '\n' + tr("Only one Starter license is allowed per System,"
                    " so the second license will be deactivated.")
                + '\n' + tr("Merge anyway?");
        case MergeSystemsStatus::nvrLicense:
            return tr("You are about to merge Systems with NVR licenses.")
                + '\n' + tr("Only one NVR license is allowed per System,"
                    " so the second license will be deactivated.")
                + '\n' + tr("Merge anyway?");
        case MergeSystemsStatus::configurationFailed:
            return tr("Could not configure the remote System %1.",
                "%1 is name of System")
                .arg(systemName);
        case MergeSystemsStatus::dependentSystemBoundToCloud:
            return tr("%1 System can only be merged with non-%1. "
                "System name and password are taken from %1 System.",
                "%1 is the short cloud name (like Cloud)")
                .arg(nx::branding::shortCloudName());
        case MergeSystemsStatus::bothSystemBoundToCloud:
            return tr("Both Systems are connected to %1. Merge is not allowed.",
                "%1 is the cloud name (like Nx Cloud)")
                .arg(nx::branding::cloudName());
        case MergeSystemsStatus::cloudSystemsHaveDifferentOwners:
            return tr("%1 systems have different owners. Merge is not allowed.",
                "%1 is the cloud name (like Nx Cloud)")
                .arg(nx::branding::cloudName());
        case MergeSystemsStatus::duplicateMediaServerFound:
            return tr("Cannot merge Systems because they have at least one server with the "
                "same ID. Please remove this server and try again.");
        case MergeSystemsStatus::unconfiguredSystem:
            return tr("System name is not configured yet.");
        case MergeSystemsStatus::certificateInvalid:
            return tr("Connection to Server could not be established."
                " The Server's certificate is invalid.");
        case MergeSystemsStatus::certificateRejected:
            return tr("Connection to Server could not be established."
                " The Server's certificate was rejected.");
        default:
            return tr("Unknown error.");
    }
}

} // namespace nx::vms::client::desktop
