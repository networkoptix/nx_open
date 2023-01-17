// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_user_interaction_delegate.h"

#include <client_core/client_core_module.h>
#include <nx/network/ssl/certificate.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/settings/client_core_settings.h>

namespace nx::vms::client::core {

using CertificateValidationLevel = network::server_certificate::ValidationLevel;

struct RemoteConnectionUserInteractionDelegate::Private
{
    const TokenValidator validateToken;
    const AskUserToAcceptCertificate askToAcceptCertificate;
    const ShowCertificateError showCertificateError;

    CertificateValidationLevel validationLevel()
    {
        return settings()->certificateValidationLevel();
    }

    Private(
        TokenValidator validateToken,
        AskUserToAcceptCertificate askToAcceptCertificate,
        ShowCertificateError showCertificateError);

    void tryShowCertificateError(
        const nx::vms::api::ModuleInformation& target,
        const nx::network::SocketAddress& primaryAddress,
        const nx::network::ssl::CertificateChain& chain);
};

RemoteConnectionUserInteractionDelegate::Private::Private(
    TokenValidator validateToken,
    AskUserToAcceptCertificate askToAcceptCertificate,
    ShowCertificateError showCertificateError)
    :
    validateToken(validateToken),
    askToAcceptCertificate(askToAcceptCertificate),
    showCertificateError(showCertificateError)
{
}

void RemoteConnectionUserInteractionDelegate::Private::tryShowCertificateError(
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress,
    const nx::network::ssl::CertificateChain& chain)
{
    const auto session = qnClientCoreModule->networkModule()->session();
    if (session && session->state() == nx::vms::client::core::RemoteSession::State::reconnecting)
        return;

    showCertificateError(target, primaryAddress, chain);
}

//-------------------------------------------------------------------------------------------------
// RemoteConnectionUserInteractionDelegate

RemoteConnectionUserInteractionDelegate::RemoteConnectionUserInteractionDelegate(
    TokenValidator validateToken,
    AskUserToAcceptCertificate askToAcceptCertificate,
    ShowCertificateError showCertificateError,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(validateToken, askToAcceptCertificate, showCertificateError))
{
    NX_ASSERT(validateToken, "Validate token handler should be specified!");
    NX_ASSERT(askToAcceptCertificate, "Ask to accept certificate handler should be specified!");
    NX_ASSERT(showCertificateError, "Show certificate error handler should be specified!");
}

RemoteConnectionUserInteractionDelegate::~RemoteConnectionUserInteractionDelegate()
{
}

bool RemoteConnectionUserInteractionDelegate::acceptNewCertificate(
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress,
    const nx::network::ssl::CertificateChain& chain)
{
    switch (d->validationLevel())
    {
        case CertificateValidationLevel::disabled:
            return true;

        case CertificateValidationLevel::recommended:
            return d->askToAcceptCertificate(target, primaryAddress, chain,
                CertificateWarning::Reason::unknownServer);

        case CertificateValidationLevel::strict:
            d->tryShowCertificateError(target, primaryAddress, chain);
            return false;

        default:
            NX_ASSERT(false, "Unreachable");
            return false;
    }
}

bool RemoteConnectionUserInteractionDelegate::acceptCertificateAfterMismatch(
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress,
    const nx::network::ssl::CertificateChain& chain)
{
    switch (d->validationLevel())
    {
        case CertificateValidationLevel::disabled:
            return true;

        case CertificateValidationLevel::recommended:
            return d->askToAcceptCertificate(target,
                primaryAddress,
                chain,
                CertificateWarning::Reason::invalidCertificate);

        case CertificateValidationLevel::strict:
            d->tryShowCertificateError(target, primaryAddress, chain);
            return false;

        default:
            NX_ASSERT(false, "Unreachable");
            return false;
    }
}

bool RemoteConnectionUserInteractionDelegate::acceptCertificateOfServerInTargetSystem(
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress,
    const nx::network::ssl::CertificateChain& chain)
{
    switch (d->validationLevel())
    {
        case CertificateValidationLevel::disabled:
            return true;

        case CertificateValidationLevel::recommended:
            return d->askToAcceptCertificate(target,
                primaryAddress,
                chain,
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
