// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "certificate_verifier.h"

#include <future>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QPointer>

#include <api/global_settings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/nx_network_ini.h>
#include <nx/network/socket_factory.h>
#include <nx/network/ssl/certificate.h>
#include <nx/network/ssl/helpers.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/ini.h>

#include "certificate_storage.h"
#include "remote_connection.h"
#include "remote_session.h"

namespace nx::vms::client::core {

using CertificateValidationLevel = ServerCertificateValidationLevel;

struct CertificateVerifier::Private
{
    Status verifyCertificateByStorage(
        const QnUuid& serverId,
        const nx::network::ssl::CertificateChain& chain,
        bool acceptExpired) const
    {
        // Normally both storages should be avaliable. If not, return an error.
        if (!NX_ASSERT(connectionCertificatesStorage)
            || !NX_ASSERT(autoGeneratedCertificatesStorage))
        {
            return Status::notFound;
        }

        if (!NX_ASSERT(!serverId.isNull()))
            return Status::notFound;

        if (!NX_ASSERT(!chain.empty()))
            return Status::notFound;

        if (certificateValidationLevel == CertificateValidationLevel::disabled)
            return Status::ok;

        const auto& certificate = chain[0];

        NX_VERBOSE(this, "Validating certificate for the server %1", serverId);

        const auto connectionCertificatePublicKey =
            connectionCertificatesStorage->pinnedCertificate(serverId);
        const auto autoGeneratedCertificatePublicKey =
            autoGeneratedCertificatesStorage->pinnedCertificate(serverId);

        if (!connectionCertificatePublicKey && !autoGeneratedCertificatePublicKey)
            return Status::notFound;

        const auto expiresAt = std::chrono::duration_cast<std::chrono::seconds>(
            certificate.notAfter().time_since_epoch());
        // Current spec says that we should not report a severe error when the certificate is shown
        // to user for the first time. Therefore we check expiration time only at this point.
        if (expiresAt.count() < QDateTime::currentSecsSinceEpoch() && !acceptExpired)
        {
            NX_VERBOSE(this, "Certificate has expired");
            return Status::mismatch;
        }

        if (connectionCertificatePublicKey == certificate.publicKey())
        {
            NX_VERBOSE(this,
                "Found pinned connection certificate: %1",
                *connectionCertificatePublicKey);
            return Status::ok;
        }
        if (autoGeneratedCertificatePublicKey == certificate.publicKey())
        {
            NX_VERBOSE(this,
                "Found pinned auto-generated certificate: %1",
                *autoGeneratedCertificatePublicKey);
            return Status::ok;
        }

        NX_VERBOSE(this,
            "Certificate mismatch for the server %1. Pinned: %2 and %3, actual: %4",
            serverId,
            connectionCertificatePublicKey,
            autoGeneratedCertificatePublicKey,
            certificate.publicKey());

        return Status::mismatch;
    }

    void pinCertificate(
        const QnUuid& serverId,
        const std::string& publicKey,
        CertificateType certType)
    {
        switch (certType)
        {
            case CertificateType::autogenerated:
                autoGeneratedCertificatesStorage->pinCertificate(serverId, publicKey);
                break;
            case CertificateType::connection:
                connectionCertificatesStorage->pinCertificate(serverId, publicKey);
                break;
            default:
                NX_ASSERT(false, "Unknown certificate type");
        }
    }

    std::optional<std::string> pinnedCertificate(
        const QnUuid& serverId,
        CertificateType certType)
    {
        switch (certType)
        {
            case CertificateType::autogenerated:
                return autoGeneratedCertificatesStorage->pinnedCertificate(serverId);
            case CertificateType::connection:
                return connectionCertificatesStorage->pinnedCertificate(serverId);
            default:
                NX_ASSERT(false, "Unknown certificate type");
                return {};
        }
    }

    std::shared_ptr<CertificateCache> certificateCache() const
    {
        if (auto session = sessionPtr.lock())
        {
            if (auto connection = session->connection(); NX_ASSERT(connection))
                return connection->certificateCache();
        }

        return {};
    }

    mutable Mutex mutex;

    CertificateValidationLevel certificateValidationLevel = CertificateValidationLevel::strict;
    QPointer<CertificateStorage> connectionCertificatesStorage;
    QPointer<CertificateStorage> autoGeneratedCertificatesStorage;

    std::weak_ptr<RemoteSession> sessionPtr;
};

CertificateVerifier::CertificateVerifier(
    ServerCertificateValidationLevel certificateValidationLevel,
    CertificateStorage* connectionCertificatesStorage,
    CertificateStorage* autoGeneratedCertificatesStorage,
    QObject* parent)
    :
    base_type(parent),
    d(new Private())
{
    d->certificateValidationLevel = certificateValidationLevel;
    d->connectionCertificatesStorage = connectionCertificatesStorage;
    d->autoGeneratedCertificatesStorage = autoGeneratedCertificatesStorage;

    if (QString path = ini().rootCertificatesFolder; !path.isEmpty())
    {
        QDir folder(path);
        if (!NX_ASSERT(folder.exists(), "Root certificates folder does not exist: %1", path))
            return;

        const auto entries = folder.entryList({"*.pem", "*.crt"}, QDir::Files);
        for (const auto& fileName: entries)
        {
            QFile f(folder.absoluteFilePath(fileName));
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                NX_WARNING(this, "Cannot open file %1", folder.absoluteFilePath(fileName));
                continue;
            }
            loadTrustedCertificate(f.readAll(), fileName);
        }
    }
}

CertificateVerifier::~CertificateVerifier()
{
}

nx::network::ssl::AdapterFunc CertificateVerifier::makeRestrictedAdapterFunc(
    const std::string& expectedKey)
{
    if (!nx::network::ini().verifyVmsSslCertificates)
        return nx::network::ssl::kAcceptAnyCertificate;

    return nx::network::ssl::makeAdapterFunc(
        [expectedKey](const nx::network::ssl::CertificateChain& chain)
        {
            return NX_ASSERT(!chain.empty()) && (chain[0].publicKey() == expectedKey);
        });
}

nx::network::ssl::AdapterFunc CertificateVerifier::makeAdapterFunc(const QnUuid& serverId)
{
    if (!nx::network::ini().verifyVmsSslCertificates)
        return nx::network::ssl::kAcceptAnyCertificate;

    NX_MUTEX_LOCKER lock(&d->mutex);
    auto cache = d->certificateCache();

    return NX_ASSERT(cache)
        ? cache->makeAdapterFunc(serverId)
        : nx::network::ssl::makeAdapterFunc(
            [](const nx::network::ssl::CertificateChain& chain){ return false; });
}

CertificateVerifier::Status CertificateVerifier::verifyCertificate(
    const QnUuid& serverId,
    const nx::network::ssl::CertificateChain& chain,
    bool acceptExpired) const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->verifyCertificateByStorage(serverId, chain, acceptExpired);
}

void CertificateVerifier::setValidationLevel(CertificateValidationLevel certificateValidationLevel)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    d->certificateValidationLevel = certificateValidationLevel;
}

void CertificateVerifier::pinCertificate(
    const QnUuid& serverId,
    const std::string& publicKey,
    CertificateType certType)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    d->pinCertificate(serverId, publicKey, certType);
}

std::optional<std::string> CertificateVerifier::pinnedCertificate(
    const QnUuid& serverId,
    CertificateType certType)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->pinnedCertificate(serverId, certType);
}

void CertificateVerifier::updateCertificate(
    const QnUuid& serverId,
    const std::string& publicKey,
    CertificateType certType)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    auto cache = d->certificateCache();
    if (!NX_ASSERT(cache))
        return;

    auto cached = cache->certificate(serverId, certType);
    auto pinned = d->pinnedCertificate(serverId, certType);

    if (cached == pinned)
        d->pinCertificate(serverId, publicKey, certType);

    cache->addCertificate(serverId, publicKey, certType);
}

void CertificateVerifier::setSession(std::weak_ptr<RemoteSession> session)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    d->sessionPtr = session;
}

void CertificateVerifier::removeCertificatesFromCache(const QnUuid& serverId)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    if (auto cache = d->certificateCache())
        cache->removeCertificates(serverId);
}

//-------------------------------------------------------------------------------------------------

struct CertificateCache::Private
{
    using CertificateType = CertificateVerifier::CertificateType;
    using Status = CertificateVerifier::Status;

    template<typename Chain>
    Status verifyCertificateByCache(
        const QnUuid& serverId,
        const Chain& chain)
    {
        if (!NX_ASSERT(!serverId.isNull()))
            return Status::notFound;

        if (!NX_ASSERT(!chain.empty()))
            return Status::notFound;

        const auto& certificate = chain[0];

        const auto connectionCertificatePublicKey = connectionCertificate(serverId);
        const auto autoGeneratedCertificatePublicKey = autoGeneratedCertificate(serverId);

        if (!connectionCertificatePublicKey && !autoGeneratedCertificatePublicKey)
            return Status::notFound;

        if (connectionCertificatePublicKey == certificate.publicKey())
            return Status::ok;

        if (autoGeneratedCertificatePublicKey == certificate.publicKey())
            return Status::ok;

        NX_WARNING(this,
            "Certificate mismatch for the server %1. Cached: %2 and %3, actual: %4",
            serverId,
            connectionCertificatePublicKey,
            autoGeneratedCertificatePublicKey,
            certificate.publicKey());

        return Status::mismatch;
    }

    std::optional<std::string> connectionCertificate(const QnUuid& serverId) const
    {
        if (auto it = connectionCertificates.find(serverId);
            it != connectionCertificates.end())
        {
            return it->second;
        }
        return {};
    }

    std::optional<std::string> autoGeneratedCertificate(const QnUuid& serverId) const
    {
        if (auto it = autoGeneratedCertificates.find(serverId);
            it != autoGeneratedCertificates.end())
        {
            return it->second;
        }
        return {};
    }

    mutable Mutex mutex;
    std::map<QnUuid, std::string> connectionCertificates;
    std::map<QnUuid, std::string> autoGeneratedCertificates;
};

CertificateCache::CertificateCache(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

CertificateCache::~CertificateCache()
{
}

void CertificateCache::addCertificate(
    const QnUuid& serverId,
    const std::string& publicKey,
    CertificateVerifier::CertificateType certType)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    switch (certType)
    {
        case CertificateVerifier::CertificateType::autogenerated:
            d->autoGeneratedCertificates[serverId] = publicKey;
            break;
        case CertificateVerifier::CertificateType::connection:
            d->connectionCertificates[serverId] = publicKey;
            break;
        default:
            NX_ASSERT(false, "Unknown certificate type");
    }
}

std::optional<std::string> CertificateCache::certificate(
    const QnUuid& serverId,
    CertificateVerifier::CertificateType certType) const
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    switch (certType)
    {
        case CertificateVerifier::CertificateType::autogenerated:
            return d->autoGeneratedCertificate(serverId);
        case CertificateVerifier::CertificateType::connection:
            return d->connectionCertificate(serverId);
        default:
            NX_ASSERT(false, "Unknown certificate type");
            return {};
    }
}

void CertificateCache::removeCertificates(const QnUuid& serverId)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    d->connectionCertificates.erase(serverId);
    d->autoGeneratedCertificates.erase(serverId);
}

nx::network::ssl::AdapterFunc CertificateCache::makeAdapterFunc(const QnUuid& serverId)
{
    if (!nx::network::ini().verifyVmsSslCertificates)
        return nx::network::ssl::kAcceptAnyCertificate;

    return nx::network::ssl::makeAdapterFunc(
        [guard = std::weak_ptr<Private>(d), serverId]
            (const nx::network::ssl::CertificateChain& chain)
        {
            // Someone can send request when the certificate cache is deleted already.
            auto d = guard.lock();
            if (!d)
                return false;

            NX_MUTEX_LOCKER lock(&d->mutex);
            return d->verifyCertificateByCache(serverId, chain) == CertificateVerifier::Status::ok;
        });
}

} // namespace nx::vms::client::core
