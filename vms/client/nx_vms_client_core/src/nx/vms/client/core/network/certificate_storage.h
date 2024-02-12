// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

#include "server_certificate_validation_level.h"

class QString;

namespace nx::vms::client::core {

/**
 * File-based storage class for the TLS Certificate Pinning.
 * Keeps record of all pinned certificates, allows to check if the provided certificate is pinned
 * or not, and pin it if needed. Only public keys are stored to identify certificates.
 */
class NX_VMS_CLIENT_CORE_API CertificateStorage: public QObject
{
    using base_type = QObject;

public:
    /** Use certain path as a storage location. */
    explicit CertificateStorage(
        const QString& storagePath,
        network::server_certificate::ValidationLevel certificateValidationLevel,
        QObject* parent = nullptr);

    virtual ~CertificateStorage() override;

    /**
     * Read pinned certificate for the given server from it's file.
     * @return nullopt if no certificate is pinned, public key otherwise.
     */
    std::optional<std::string> pinnedCertificate(const nx::Uuid& serverId) const;

    /**
     * Store given certificate's public key, replacing existing if present.
     */
    void pinCertificate(const nx::Uuid& serverId, const std::string& publicKey);

    /**
     * Clears the storage, then initializes it with the given settings.
     */
    void reinitialize(network::server_certificate::ValidationLevel certificateValidationLevel);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
