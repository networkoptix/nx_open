// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>

#include <QtCore/QObject>

#include <nx/vms/common/network/abstract_certificate_verifier.h>

#include <nx/network/ssl/helpers.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

class CertificateStorage;
class RemoteSession;

/**
 * Ensures certificate can be used to establish connection. User interaction requested if needed,
 * confirmed certificates are pinned.
 */
class CertificateVerifier: public nx::vms::common::AbstractCertificateVerifier
{
    Q_OBJECT
    using base_type = nx::vms::common::AbstractCertificateVerifier;

public:
    enum class CertificateType
    {
        autogenerated,
        connection,
    };

    enum class Status
    {
        ok,
        notFound,
        mismatch,
    };

    /**
     * Verifier will use given storages to check if the provided non-public certificate is
     * auto-generated by the server (if we already connected to the system) or is manually pinned by
     * the user. Accepted certificates are pinned to the connection certificates storage.
     * Ownership is not passed, caller must ensure correct destruction order.
     */
    CertificateVerifier(
        CertificateStorage* connectionCertificatesStorage,
        CertificateStorage* autoGeneratedCertificatesStorage,
        QObject* parent = nullptr);

    virtual ~CertificateVerifier() override;

    /**
     * Create AdapterFunc that accepts only certificates with matching public key. This function
     * is intended to be used when establishing a connection.
     */
    nx::network::ssl::AdapterFunc makeRestrictedAdapterFunc(const std::string& expectedKey);

    /**
     * Create AdapterFunc that uses the active connection certificate cache.
     */
    virtual nx::network::ssl::AdapterFunc makeAdapterFunc(const QnUuid& serverId) override;

    /**
     * Check certificate chain using certificate keys in persistent storage.
     */
    Status verifyCertificate(
        const QnUuid& serverId,
        const nx::network::ssl::CertificateChain& chain,
        bool acceptExpired = false) const;

    /**
     * Store given certificate's public key, replacing existing if present.
     */
    void pinCertificate(
        const QnUuid& serverId,
        const std::string& publicKey,
        CertificateType certType = CertificateType::connection);

    std::optional<std::string> pinnedCertificate(
        const QnUuid& serverId,
        CertificateType certType);

    /**
     * Updates cached certificate in memory. If the cached certificate matched a pinned certiifcate
     * of the server before the call, updates the pinned certificate too.
     */
    void updateCertificate(
        const QnUuid& serverId,
        const std::string& publicKey,
        CertificateType certType);

    void removeCertificatesFromCache(const QnUuid& serverId);

    void setSession(std::weak_ptr<RemoteSession> session);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

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
        const QnUuid& serverId,
        const std::string& publicKey,
        CertificateVerifier::CertificateType certType);

    std::optional<std::string> certificate(
        const QnUuid& serverId,
        CertificateVerifier::CertificateType certType) const;

    void removeCertificates(const QnUuid& serverId);

    virtual nx::network::ssl::AdapterFunc makeAdapterFunc(const QnUuid& serverId) override;

private:
    struct Private;
    std::shared_ptr<Private> d;
};

} // namespace nx::vms::client::core
