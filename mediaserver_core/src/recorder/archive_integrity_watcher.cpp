#include <nx/utils/log/log.h>
#include "archive_integrity_watcher.h"
#include "storage_manager.h"
#include "server_stream_recorder.h"

namespace nx {
namespace mediaserver {


std::chrono::milliseconds kMinTimeoutBetweenResets{100};

// IntegrityHashHelper -----------------------------------------------------------------------------
const QByteArray IntegrityHashHelper::kIntegrityHashSalt = "408422e1-1b4c-498c-b45a-43ef7723c6e5";

QByteArray IntegrityHashHelper::generateIntegrityHash(const QByteArray& value)
{
    return hashWithSalt(value);
}

bool IntegrityHashHelper::checkIntegrity(
    const QByteArray& initialValue,
    const QByteArray& hashedValue)
{
    return hashWithSalt(initialValue) == hashedValue;
}

QByteArray IntegrityHashHelper::hashWithSalt(const QByteArray& value)
{
    nx::utils::QnCryptographicHash hashGenerator(nx::utils::QnCryptographicHash::Md5);
    hashGenerator.addData(value);
    hashGenerator.addData(kIntegrityHashSalt);

    return hashGenerator.result();
}
// -------------------------------------------------------------------------------------------------

// ServerArchiveIntegrityWatcher -------------------------------------------------------------------
ServerArchiveIntegrityWatcher::ServerArchiveIntegrityWatcher():
    m_fired(false),
    m_lastFireTime(std::chrono::steady_clock::time_point::min())
{}

bool ServerArchiveIntegrityWatcher::fileRequested(
    const QnAviArchiveMetadata& metadata,
    const QString& fileName)
{
    if (!checkMetaDataIntegrity(metadata))
    {
        emitSignal(fileName);
        NX_WARNING(this, lm("File metadata integrity problem: %1").args(fileName));
        return false;
    }

    if (!checkFileName(metadata, fileName))
    {
        emitSignal(fileName);
        NX_WARNING(this, lm("File metadata vs file name integrity problem: %1").args(fileName));
        return false;
    }

    return true;
}

void ServerArchiveIntegrityWatcher::fileMissing(const QString& fileName)
{
    NX_WARNING(this, lm("File is present in the DB but missing in the archive: %1").args(fileName));
    emitSignal(fileName);
}

void ServerArchiveIntegrityWatcher::reset()
{
    using namespace std::chrono;

    auto now = std::chrono::steady_clock::now();
    if (tooEarlyToReset(now))
        return;

    m_fired = false;
    m_lastFireTime = now;
}

bool ServerArchiveIntegrityWatcher::tooEarlyToReset(std::chrono::steady_clock::time_point now) const
{
    if (m_lastFireTime == std::chrono::steady_clock::time_point::min())
        return false;

    return now - m_lastFireTime < kMinTimeoutBetweenResets;
}

bool ServerArchiveIntegrityWatcher::checkMetaDataIntegrity(const QnAviArchiveMetadata& metadata)
{
    QByteArray initialValue = QByteArray::number(metadata.startTimeMs);
    return IntegrityHashHelper::checkIntegrity(initialValue, metadata.integrityHash);
}

bool ServerArchiveIntegrityWatcher::checkFileName(
    const QnAviArchiveMetadata& metadata,
    const QString& fileName)
{
    return fileName.contains(QString::number(metadata.startTimeMs));
}

void ServerArchiveIntegrityWatcher::emitSignal(const QString& fileName)
{
    if (m_fired)
        return;

    qWarning() << "REPORT";
    QnStorageResourcePtr storage = QnStorageManager::getStorageByUrl(
        fileName,
        QnServer::StoragePool::Backup | QnServer::StoragePool::Normal);

    NX_ASSERT(storage);
    if (!storage)
        return;

    emit fileIntegrityCheckFailed(storage);
    m_fired = true;
}
// -------------------------------------------------------------------------------------------------

} // namespace mediaserver
} // namespace nx
