// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/url.h>
#include <utils/merge_systems_common.h>

#include "merge_system_requests.h"

namespace nx::vms::client::core {
class AbstractRemoteConnectionUserInteractionDelegate;
class CertificateVerifier;
} // namespace nx::vms::client::core

namespace nx::vms::client::desktop {

class MergeSystemRequestsManager;

/**
 * Implements system merge logic.
 *
 * Current merge logic may contain up to 4 main steps:
 * 1. Get target server info with certificate.
 * 1.1. Approve target server certificate by the user.
 * 2. Get target server bearer token in exchange for user/password.
 * 3. Get target license list if need.
 * 3.1 Show license deactivation warning in case of license conflict.
 * 4. Send merge request to current system, using target info, token and certificate.
 *
 * Requests in steps 1, 2 and 3 are proxified to target server via currently connected one.
 */
class MergeSystemsTool: public QObject
{
    Q_OBJECT

public:
    using CertificateVerifier = nx::vms::client::core::CertificateVerifier;
    using Delegate = nx::vms::client::core::AbstractRemoteConnectionUserInteractionDelegate;

public:
    struct DryRunSettings
    {
        bool ownSettings = false;
        bool oneServer = false;
        bool ignoreIncompatible = false;
    };

public:
    MergeSystemsTool(
        QObject* parent,
        CertificateVerifier* certificateVerifier,
        Delegate* delegate,
        const std::string& locale);
    ~MergeSystemsTool();

    /**
     * Obtains target server info by it's address. Validates licenses. Tries merge in dry run mode
     * if setting are provided.
     * @param proxy Current system server resource.
     * @param targetUrl Target server API url.
     * @param dryRunSettings Settings for merge in dry run mode.
     * @return Ping id which can be used for subsequent merge.
     */
    [[nodiscard]]
    QnUuid pingSystem(
        const QnMediaServerResourcePtr& proxy,
        const nx::utils::Url& targetUrl,
        const nx::network::http::Credentials& targetCredentials,
        std::optional<DryRunSettings> dryRunSettings = {});

    /**
     * Continues merge process after successful ping.
     * @param ctxId Context id returned by ping method.
     * @param ownerSessionToken Fresh owner session token.
     * @param ownSettings User current system settings if true.
     * @param oneServer Connect to current system target server only.
     * @param ignoreIncompatible Ignore incompatible servers while merge.
     */
    bool mergeSystem(
        const QnUuid& ctxId,
        const std::string& ownerSessionToken,
        bool ownSettings,
        bool oneServer = false,
        bool ignoreIncompatible = false);

    static QString getErrorMessage(
        MergeSystemsStatus value,
        const nx::vms::api::ModuleInformation& moduleInformation);

signals:
    void systemFound(
        MergeSystemsStatus mergeStatus,
        const QString& errorText,
        const nx::vms::api::ModuleInformation& moduleInformation,
        const nx::vms::api::MergeStatusReply& reply);
    void mergeFinished(
        MergeSystemsStatus mergeStatus,
        const QString& errorText,
        const nx::vms::api::ModuleInformation& moduleInformation);

private:
    struct Context;

    Context* findContext(const QnUuid& ctxId);

    static bool checkServerCertificateEquality(const Context& ctx);
    MergeSystemsStatus verifyTargetCertificate(const Context& ctx);

    void reportMergeFinished(
        const Context& ctx,
        MergeSystemsStatus status,
        const QString& errorText = QString());

    void reportSystemFound(
        const Context& ctx,
        MergeSystemsStatus status,
        const QString& errorText = QString(),
        bool removeCtx = true,
        const nx::vms::api::MergeStatusReply& reply = {});

    void mergeSystemDryRun(const Context& ctx);

    void at_serverInfoReceived(
        Context& ctx, const rest::ErrorOrData<nx::vms::api::ServerInformation>& errorOrData);

    void at_sessionCreated(
        Context& ctx, const rest::ErrorOrData<nx::vms::api::LoginSession>& errorOrData);

    void at_licensesReceived(
        Context& ctx, const rest::ErrorOrData<nx::vms::api::LicenseDataList>& errorOrData);

    void at_mergeStarted(
        Context& ctx, const rest::ErrorOrData<nx::vms::api::MergeStatusReply>& errorOrData);

private:
    std::unique_ptr<MergeSystemRequestsManager> m_requestManager;
    std::map<QnUuid, Context> m_contextMap;
    CertificateVerifier* m_certificateVerifier;
    Delegate* m_delegate;
};

} // namespace nx::vms::client::desktop
