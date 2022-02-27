// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "merge_systems_tool.h"

#include <common/common_module.h>
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
    const nx::network::http::Credentials& targetCredentials)
{
    auto ctxId = QnUuid::createUuid();

    Context ctx;
    ctx.id = ctxId;
    ctx.proxy = proxy;
    ctx.target = nx::network::SocketAddress::fromUrl(targetUrl);
    ctx.targetUser = targetCredentials.username;
    ctx.targetPassword = targetCredentials.authToken.value;

    NX_DEBUG(this, "Starting ping server at address %1", ctx.target);

    auto onTargetInfo =
        [this, ctxId](RestResultOrData<nx::vms::api::ServerInformation> resultOrData)
        {
            if (auto ctx = findContext(ctxId))
                at_serverInfoReceived(*ctx, resultOrData);
        };

    auto certificateSaveFunc =
        [this, ctxId](
            const nx::network::ssl::CertificateChain& chain)
        {
            if (auto ctx = findContext(ctxId); ctx && !chain.empty())
            {
                ctx->targetHandshakeChain = chain;
                return true;
            }
            return false;
        };

    m_requestManager->getTargetInfo(
        ctx.proxy,
        m_certificateVerifier->makeAdapterFunc(ctx.proxy->getId()),
        ctx.target,
        nx::network::ssl::makeAdapterFunc(std::move(certificateSaveFunc)),
        nx::utils::guarded(this, std::move(onTargetInfo)));
    m_contextMap.emplace(ctxId, ctx);
    return ctxId;
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

    auto onMergeStarted =
        [this, ctxId](const RestResultOrData<nx::vms::api::MergeStatusReply>& resultOrData)
        {
            if (auto ctx = findContext(ctxId))
                at_mergeStarted(*ctx, resultOrData);
        };

    m_requestManager->mergeSystem(
        ctx->proxy,
        m_certificateVerifier->makeAdapterFunc(ctx->proxy->getId()),
        ownerSessionToken,
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
    auto targetInfo = ctx.targetInfo;
    m_contextMap.erase(ctx.mergeData.remoteServerId);

    emit mergeFinished(status, errorText, targetInfo);
}

void MergeSystemsTool::reportSystemFound(
    const Context& ctx,
    MergeSystemsStatus status,
    const QString& errorText,
    bool removeCtx)
{
    auto targetInfo = ctx.targetInfo;

    if (removeCtx)
        m_contextMap.erase(ctx.mergeData.remoteServerId);

    emit systemFound(status, errorText, targetInfo);
}

void MergeSystemsTool::at_serverInfoReceived(
    Context& ctx,
    const RestResultOrData<nx::vms::api::ServerInformation>& resultOrData)
{
    if (auto serverInfo = std::get_if<nx::vms::api::ServerInformation>(&resultOrData))
    {
        ctx.targetInfo = std::move(*serverInfo);

        if (auto status = verifyTargetCertificate(ctx); status != MergeSystemsStatus::ok)
        {
            reportSystemFound(ctx, status, {}, true);
            return;
        }

        auto onSessionCreated =
            [this, ctxId = ctx.id](
                RestResultOrData<nx::vms::api::LoginSession> resultOrData)
            {
                if (auto ctx = findContext(ctxId))
                    at_sessionCreated(*ctx, resultOrData);
            };

        nx::vms::api::LoginSessionRequest request;
        request.username = QString::fromStdString(ctx.targetUser);
        request.password = QString::fromStdString(ctx.targetPassword);

        m_requestManager->createTargetSession(
            ctx.proxy,
            m_certificateVerifier->makeAdapterFunc(ctx.proxy->getId()),
            ctx.target,
            m_certificateVerifier->makeRestrictedAdapterFunc(ctx.targetPublicKey()),
            request,
            nx::utils::guarded(this, std::move(onSessionCreated)));
    }
    else if (auto restResult = std::get_if<nx::network::rest::Result>(&resultOrData))
    {
        NX_WARNING(this, "Can't get server info, rest result: %1", QJson::serialized(*restResult));
        // Older versions responds with {"error" : 404 } special processing is required.
        if ((static_cast<nx::network::http::StatusCode::Value>(restResult->error)
            == nx::network::http::StatusCode::notFound)
            || (restResult->error == nx::network::rest::Result::NotFound))
        {
            reportSystemFound(ctx, MergeSystemsStatus::incompatibleInternal, QString(), true);
        }
        else
        {
            reportSystemFound(
                ctx, mergeStatusFromResult(*restResult), restResult->errorString, true);
        }
    }
}

void MergeSystemsTool::at_sessionCreated(
    Context& ctx,
    const RestResultOrData<nx::vms::api::LoginSession>& resultOrData)
{
    if (auto loginSession = std::get_if<nx::vms::api::LoginSession>(&resultOrData))
    {
        NX_ASSERT(!loginSession->token.empty());
        ctx.mergeData.remoteSessionToken = std::move(loginSession->token);

        if (!nx::vms::license::hasUniqueLicenses(ctx.proxy->commonModule()->licensePool()))
        {
            reportSystemFound(ctx, MergeSystemsStatus::ok, {}, false);
            return;
        }

        auto onLicenseReceived =
            [this, ctxId = ctx.id](
                const RestResultOrData<nx::vms::api::LicenseDataList>& resultOrData)
        {
            if (auto ctx = findContext(ctxId))
                at_licensesReceived(*ctx, resultOrData);
        };

        m_requestManager->getTargetLicenses(
            ctx.proxy,
            m_certificateVerifier->makeAdapterFunc(ctx.proxy->getId()),
            ctx.target,
            ctx.mergeData.remoteSessionToken,
            m_certificateVerifier->makeRestrictedAdapterFunc(ctx.targetPublicKey()),
            std::move(onLicenseReceived)
        );

        return;
    }
    else if (auto restResult = std::get_if<nx::network::rest::Result>(&resultOrData))
    {
        NX_WARNING(this, "Can't create session, rest result: %1", QJson::serialized(*restResult));
        reportSystemFound(ctx, mergeStatusFromResult(*restResult), restResult->errorString, true);
    }
}

void MergeSystemsTool::at_licensesReceived(
    Context& ctx,
    const RestResultOrData<nx::vms::api::LicenseDataList>& resultOrData)
{
    if (auto licenseDataList = std::get_if<nx::vms::api::LicenseDataList>(&resultOrData))
    {
        ec2::fromApiToResourceList(*licenseDataList, ctx.targetLicenses);
        auto status = nx::vms::license::remoteLicensesConflict(
            ctx.proxy->commonModule()->licensePool(),
            ctx.targetLicenses);

        reportSystemFound(ctx, status, {}, false);
    }
    else if (auto restResult = std::get_if<nx::network::rest::Result>(&resultOrData))
    {
        NX_WARNING(this, "Can't get licenses, rest result: %1", QJson::serialized(*restResult));
        reportSystemFound(ctx, mergeStatusFromResult(*restResult), restResult->errorString, true);
    }
}

void MergeSystemsTool::at_mergeStarted(
    Context& ctx,
    const RestResultOrData<nx::vms::api::MergeStatusReply>& resultOrData)
{
    if (auto mergeStatus = std::get_if<nx::vms::api::MergeStatusReply>(&resultOrData))
    {
        if (mergeStatus->mergeInProgress)
            reportMergeFinished(ctx, MergeSystemsStatus::ok);
        else
            reportMergeFinished(ctx, MergeSystemsStatus::unknownError);
    }
    else if (auto restResult = std::get_if<nx::network::rest::Result>(&resultOrData))
    {
        NX_WARNING(this, "Can't merge systems, rest result: %1", QJson::serialized(*restResult));
        reportMergeFinished(ctx, mergeStatusFromResult(*restResult), restResult->errorString);
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

    if (!checkServerCertificateEquality(ctx))
        return MergeSystemsStatus::certificateInvalid;

    const CertificateVerifier::Status status = m_certificateVerifier->verifyCertificate(
        ctx.targetInfo.id,
        ctx.targetHandshakeChain);

    if (status == CertificateVerifier::Status::ok)
        return MergeSystemsStatus::ok;

    const auto serverName = ctx.target.address.toString();
    NX_VERBOSE(this, "Try to verify target cert from %1 by system CA certificates.", serverName);

    std::string errorMessage;
    if (nx::network::ssl::verifyBySystemCertificates(
            ctx.targetHandshakeChain, serverName, &errorMessage))
    {
        NX_VERBOSE(this, "Certificate verification for %1 is successful.", serverName);
        m_certificateVerifier->pinCertificate(
            ctx.targetInfo.id,
            ctx.targetHandshakeChain[0].publicKey());

        return MergeSystemsStatus::ok;
    }

    NX_VERBOSE(this, "System CA verification result: %1", errorMessage);

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
            ? L'"' + tr("New System") + L'"'
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
                + L'\n' + tr("Only one Starter license is allowed per System,"
                    " so the second license will be deactivated.")
                + L'\n' + tr("Merge anyway?");
        case MergeSystemsStatus::nvrLicense:
            return tr("You are about to merge Systems with NVR licenses.")
                + L'\n' + tr("Only one NVR license is allowed per System,"
                    " so the second license will be deactivated.")
                + L'\n' + tr("Merge anyway?");
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
