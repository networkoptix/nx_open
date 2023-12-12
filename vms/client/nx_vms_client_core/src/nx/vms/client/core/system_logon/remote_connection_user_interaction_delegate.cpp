// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_user_interaction_delegate.h"

#include <client_core/client_core_module.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/settings/client_core_settings.h>

namespace nx::vms::client::core {

TargetCertificateInfo::TargetCertificateInfo(
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& address,
    const nx::network::ssl::CertificateChain& chain)
    :
    target(target),
    address(address),
    chain(chain)
{
}

using CertificateValidationLevel = network::server_certificate::ValidationLevel;

struct RemoteConnectionUserInteractionDelegate::Private
{
    const TokenValidator validateToken;
    const AskUserToAcceptCertificates askToAcceptCertificates;
    const ShowCertificateError showCertificateError;

    CertificateValidationLevel validationLevel()
    {
        return appContext()->coreSettings()->certificateValidationLevel();
    }

    Private(
        TokenValidator validateToken,
        AskUserToAcceptCertificates askToAcceptCertificates,
        ShowCertificateError showCertificateError);

    void tryShowCertificateError(
        const TargetCertificateInfo& certificateInfo);
};

RemoteConnectionUserInteractionDelegate::Private::Private(
    TokenValidator validateToken,
    AskUserToAcceptCertificates askToAcceptCertificates,
    ShowCertificateError showCertificateError)
    :
    validateToken(validateToken),
    askToAcceptCertificates(askToAcceptCertificates),
    showCertificateError(showCertificateError)
{
}

void RemoteConnectionUserInteractionDelegate::Private::tryShowCertificateError(
    const TargetCertificateInfo& certificateInfo)
{
    const auto session = qnClientCoreModule->networkModule()->session();
    if (session && session->state() == nx::vms::client::core::RemoteSession::State::reconnecting)
        return;

    showCertificateError(certificateInfo);
}

//-------------------------------------------------------------------------------------------------
// RemoteConnectionUserInteractionDelegate

RemoteConnectionUserInteractionDelegate::RemoteConnectionUserInteractionDelegate(
    TokenValidator validateToken,
    AskUserToAcceptCertificates askToAcceptCertificates,
    ShowCertificateError showCertificateError,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(validateToken, askToAcceptCertificates, showCertificateError))
{
    NX_ASSERT(validateToken, "Validate token handler should be specified!");
    NX_ASSERT(askToAcceptCertificates, "Ask to accept certificates handler should be specified!");
    NX_ASSERT(showCertificateError, "Show certificate error handler should be specified!");
}

RemoteConnectionUserInteractionDelegate::~RemoteConnectionUserInteractionDelegate()
{
}

bool RemoteConnectionUserInteractionDelegate::acceptNewCertificate(
    const TargetCertificateInfo& certificateInfo)
{
    switch (d->validationLevel())
    {
        case CertificateValidationLevel::disabled:
            return true;

        case CertificateValidationLevel::recommended:
            return d->askToAcceptCertificates(
                {certificateInfo},
                CertificateWarning::Reason::unknownServer);

        case CertificateValidationLevel::strict:
            d->tryShowCertificateError(certificateInfo);
            return false;

        default:
            NX_ASSERT(false, "Unreachable");
            return false;
    }
}

bool RemoteConnectionUserInteractionDelegate::acceptCertificateAfterMismatch(
    const TargetCertificateInfo& certificateInfo)
{
    switch (d->validationLevel())
    {
        case CertificateValidationLevel::disabled:
            return true;

        case CertificateValidationLevel::recommended:
            return d->askToAcceptCertificates(
                {certificateInfo},
                CertificateWarning::Reason::invalidCertificate);

        case CertificateValidationLevel::strict:
            d->tryShowCertificateError(certificateInfo);
            return false;

        default:
            NX_ASSERT(false, "Unreachable");
            return false;
    }
}

bool RemoteConnectionUserInteractionDelegate::acceptCertificatesOfServersInTargetSystem(
    const QList<TargetCertificateInfo>& certificatesInfo)
{
    switch (d->validationLevel())
    {
        case CertificateValidationLevel::disabled:
            return true;

        case CertificateValidationLevel::recommended:
            return d->askToAcceptCertificates(
                certificatesInfo,
                CertificateWarning::Reason::serverCertificateChanged);

        case CertificateValidationLevel::strict:
            // We don't pin certificates in strict mode, so no warning is shown.
            return true;

        default:
            NX_ASSERT(false, "Unreachable");
            return false;
    }
}

bool RemoteConnectionUserInteractionDelegate::request2FaValidation(
    const nx::network::http::Credentials& credentials)
{
    return d->validateToken(credentials);
}

} // namespace nx::vms::client::core
