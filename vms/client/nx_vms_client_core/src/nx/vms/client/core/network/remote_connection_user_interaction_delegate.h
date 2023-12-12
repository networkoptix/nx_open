// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <nx/network/ssl/certificate.h>
#include <nx/network/ssl/helpers.h>
#include <nx/vms/api/data/module_information.h>

namespace nx::network { class SocketAddress; }
namespace nx::network::http { class Credentials; }

namespace nx::vms::client::core {

struct NX_VMS_CLIENT_CORE_API TargetCertificateInfo
{
public:
    TargetCertificateInfo(
        const nx::vms::api::ModuleInformation& target,
        const nx::network::SocketAddress& primaryAddress,
        const nx::network::ssl::CertificateChain& chain);

    nx::vms::api::ModuleInformation target;
    nx::network::SocketAddress address;
    nx::network::ssl::CertificateChain chain;
};

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
        const TargetCertificateInfo& certificateInfo) = 0;

    /**
     * Show warning about pinned certificate mismatch. Will be executed in the delegate's thread.
     * @return Whether the certificate must be accepted and pinned instead of the old one.
     */
    virtual bool acceptCertificateAfterMismatch(
        const TargetCertificateInfo& certificateInfo) = 0;

    /**
     * Show warning about pinned certificate mismatch. Will be executed in the delegate's thread.
     * @return Whether the certificate must be accepted and pinned instead of the old one.
     */
    virtual bool acceptCertificatesOfServersInTargetSystem(
        const QList<TargetCertificateInfo>& certificatesInfo) = 0;

    /**
     * Show OAUTH dialog with request for second factor for cloud access token validation.
     * Should be executed in the delegate's thread.
     * @return Validation success.
     */
    virtual bool request2FaValidation(const nx::network::http::Credentials& credentials) = 0;
};

} // namespace nx::vms::client::core
