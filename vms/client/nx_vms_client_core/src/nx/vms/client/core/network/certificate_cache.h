// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>

#include <QtCore/QObject>

#include <nx/network/ssl/helpers.h>
#include <nx/utils/uuid.h>
#include <nx/vms/common/network/abstract_certificate_verifier.h>

#include "certificate_verifier.h"

namespace nx::vms::client::core {

/**
 * Per-session certificate cache.
 */
class CertificateCache: public nx::vms::common::AbstractCertificateVerifier
{
    Q_OBJECT
    using base_type = nx::vms::common::AbstractCertificateVerifier;

public:
    CertificateCache(QObject* parent = nullptr);
    virtual ~CertificateCache() override;

    void addCertificate(
        const nx::Uuid& serverId,
        const std::string& publicKey,
        CertificateVerifier::CertificateType certType);

    std::optional<std::string> certificate(
        const nx::Uuid& serverId,
        CertificateVerifier::CertificateType certType) const;

    void removeCertificates(const nx::Uuid& serverId);

    virtual nx::network::ssl::AdapterFunc makeAdapterFunc(
        const nx::Uuid& serverId, const nx::Url& url = {}) override;

private:
    struct Private;
    std::shared_ptr<Private> d;
};

} // namespace nx::vms::client::core
