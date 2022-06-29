// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/network/ssl/helpers.h>

namespace nx::vms::api { struct ModuleInformation; }
namespace nx::network { class SocketAddress; }
namespace nx::network::http { class Credentials; }

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API AbstractRemoteConnectionUserInteractionDelegate: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    AbstractRemoteConnectionUserInteractionDelegate(QObject* parent = nullptr):
        QObject(parent)
    {
    }

    /**
     * Show certificate info and suggest user to pin it. Will be executed in the delegate's thread.
     * @return Whether the certificate is accepted.
     */
    virtual bool acceptNewCertificate(
        const nx::vms::api::ModuleInformation& target,
        const nx::network::SocketAddress& primaryAddress,
        const nx::network::ssl::CertificateChain& chain) = 0;

    /**
     * Show warning about pinned certificate mismatch. Will be executed in the delegate's thread.
     * @return Whether the certificate must be accepted and pinned instead of the old one.
     */
    virtual bool acceptCertificateAfterMismatch(
        const nx::vms::api::ModuleInformation& target,
        const nx::network::SocketAddress& primaryAddress,
        const nx::network::ssl::CertificateChain& chain) = 0;

    /**
     * Show warning about pinned certificate mismatch. Will be executed in the delegate's thread.
     * @return Whether the certificate must be accepted and pinned instead of the old one.
     */
    virtual bool acceptCertificateOfServerInTargetSystem(
        const nx::vms::api::ModuleInformation& target,
        const nx::network::SocketAddress& primaryAddress,
        const nx::network::ssl::CertificateChain& chain) = 0;

    /**
     * Show OAUTH dialog with request for second factor for cloud access token validation.
     * Should be executed in the delegate's thread.
     * @return Validation success.
     */
    virtual bool request2FaValidation(const nx::network::http::Credentials& credentials) = 0;
};

} // namespace nx::vms::client::core
