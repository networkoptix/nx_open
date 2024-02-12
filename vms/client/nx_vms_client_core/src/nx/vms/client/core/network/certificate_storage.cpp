// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "certificate_storage.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QString>

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

namespace {

const QString kFileNameTemplate = "%1.key";

using ServerCertificateValidationLevel =
    nx::vms::client::core::network::server_certificate::ValidationLevel;

bool isStoragePersistent(ServerCertificateValidationLevel level)
{
    return level == ServerCertificateValidationLevel::recommended;
}

} // namespace

namespace nx::vms::client::core {

struct CertificateStorage::Private
{
    mutable Mutex mutex;

    QDir dataDir;

    bool persistent{false};

    QFile keyFile(const nx::Uuid& serverId) const
    {
        return QFile(dataDir.filePath(kFileNameTemplate.arg(serverId.toSimpleString())));
    }
};

CertificateStorage::CertificateStorage(
    const QString& storagePath,
    ServerCertificateValidationLevel certificateValidationLevel,
    QObject* parent)
    :
    base_type(parent),
    d(new Private())
{
    NX_DEBUG(this, "Initialize in the %1", storagePath);
    d->dataDir = QDir(storagePath);
    d->persistent = isStoragePersistent(certificateValidationLevel);
    NX_ASSERT(d->dataDir.mkpath(storagePath), "Path %1 cannot be created", storagePath);
}

CertificateStorage::~CertificateStorage()
{
}

std::optional<std::string> CertificateStorage::pinnedCertificate(const nx::Uuid& serverId) const
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    if (!d->persistent)
        return std::nullopt;

    QFile keyFile = d->keyFile(serverId);
    if (!keyFile.exists() || !keyFile.open(QIODevice::ReadOnly))
        return std::nullopt;

    return keyFile.readAll().toStdString();
}

void CertificateStorage::pinCertificate(const nx::Uuid& serverId, const std::string& publicKey)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    if (!d->persistent)
        return;

    NX_DEBUG(this, "Pin certificate %1 to server %2", publicKey, serverId);

    QFile keyFile = d->keyFile(serverId);
    if (NX_ASSERT(keyFile.open(QIODevice::WriteOnly),
        "Certificate %1 to server %2 cannot be pinned", publicKey, serverId))
    {
        keyFile.write(QByteArray::fromStdString(publicKey));
    }
}

void CertificateStorage::reinitialize(ServerCertificateValidationLevel certificateValidationLevel)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    d->persistent = isStoragePersistent(certificateValidationLevel);

    QDirIterator it(d->dataDir.absolutePath(), { kFileNameTemplate.arg("*") });
    while (it.hasNext())
    {
        d->dataDir.remove(it.next());
    }
}

} // namespace nx::vms::client::core
