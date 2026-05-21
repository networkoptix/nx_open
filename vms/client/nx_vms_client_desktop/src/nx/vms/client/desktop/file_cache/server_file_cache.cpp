// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_file_cache.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/stored_file_data.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/remote_session.h>
#include <nx/vms/client/desktop/file_cache/file_cache_utils.h>
#include <nx/vms/common/system_settings.h>

namespace nx::vms::client::desktop {

ServerFileCache::ServerFileCache(
    SystemContext* systemContext,
    const QString& folderName,
    QObject* parent)
    :
    FileCache(parent),
    SystemContextAware(systemContext),
    m_folderName(folderName)
{
}

ServerFileCache::~ServerFileCache() = default;

void ServerFileCache::getFileList()
{
    const auto api = connectedServerApi();
    if (!api)
    {
        emitQueued(&ServerFileCache::fileListReceived,
            QStringList(), OperationResult::disconnected);
        return;
    }

    api->getStoredFiles(
        [this](bool success,
            rest::Handle /*requestId*/,
            const rest::ErrorOrData<nx::vms::api::StoredFileDataList>& result)
        {
            if (!success || !result)
            {
                emit fileListReceived({}, OperationResult::serverError);
                return;
            }

            QStringList filenames;
            for (const auto& file : *result)
                filenames << QFileInfo(file.path).fileName();

            emit fileListReceived(filenames, OperationResult::ok);
        },
        this);
}

QString ServerFileCache::cacheFolder() const
{
    // Prefix the system id so different systems don't collide and the path doesn't clash with
    // peer folders like 'log'.
    const auto systemId = globalSettings()->localSystemId();
    const QString systemName =
        '_' + nx::utils::replaceNonFileNameCharacters(systemId.toString(QUuid::WithBraces), '_');

    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir::toNativeSeparators(QString("%1/cache/%2/%3")
        .arg(root)
        .arg(systemName)
        .arg(m_folderName));
}

QString ServerFileCache::absoluteFilePath(const QString& unsafeFilename) const
{
    const QString safeFilename = file_cache::sanitizeFilename(unsafeFilename);
    if (safeFilename.isEmpty())
        return {};

    return QDir::toNativeSeparators(cacheFolder() + QDir::separator() + safeFilename);
}

QString ServerFileCache::folderName() const
{
    return m_folderName;
}

QString ServerFileCache::relativeFilePath(const QString& filename) const
{
    return nx::format("%1/%2", m_folderName, filename);
}

bool ServerFileCache::isConnectedToServer() const
{
    return connection().get() != nullptr;
}

void ServerFileCache::downloadFile(const QString& unsafeFilename)
{
    const auto api = connectedServerApi();
    if (!api)
    {
        emitQueued(&ServerFileCache::fileDownloaded,
            unsafeFilename, OperationResult::disconnected, QString());
        return;
    }

    const auto safeFilename = file_cache::sanitizeFilename(unsafeFilename);
    if (safeFilename.isEmpty())
    {
        NX_WARNING(this, "Rejecting unsafe filename: %1", unsafeFilename);
        emitQueued(&ServerFileCache::fileDownloaded,
            unsafeFilename, OperationResult::invalidOperation, QString());
        return;
    }

    if (m_deleting.contains(safeFilename))
    {
        emitQueued(&ServerFileCache::fileDownloaded,
            safeFilename, OperationResult::invalidOperation, QString());
        return;
    }

    const auto safeFilePath = absoluteFilePath(safeFilename);
    if (!NX_ASSERT(!safeFilePath.isEmpty(),
        "Sanitized file name should always produce a path: %1", unsafeFilename))
    {
        return;
    }

    if (QFileInfo::exists(safeFilePath))
    {
        emitQueued(&ServerFileCache::fileDownloaded,
            safeFilename, OperationResult::ok, safeFilePath);
        return;
    }

    if (m_loading.contains(safeFilename))
        return;

    const auto handle = api->getStoredFile(
        relativeFilePath(safeFilename),
        [this, filename = safeFilename, safeFilePath](
            bool success,
            rest::Handle requestId,
            const rest::ErrorOrData<nx::vms::api::StoredFileData>& result)
        {
            const auto it = m_loading.find(filename);
            if (it == m_loading.end() || it.value() != requestId)
                return;
            m_loading.erase(it);

            if (!isConnectedToServer())
            {
                emit fileDownloaded(filename, OperationResult::disconnected, QString());
                return;
            }

            if (!success || !result || result->data.size() <= 0)
            {
                emit fileDownloaded(filename, OperationResult::serverError, QString());
                return;
            }

            ensureCacheFolder();

            QSaveFile file(safeFilePath);
            if (!file.open(QIODevice::WriteOnly))
            {
                emit fileDownloaded(filename, OperationResult::fileSystemError, QString());
                return;
            }

            if (file.write(result->data) != result->data.size())
            {
                // QSaveFile destructor discards the temporary file when commit() is not called.
                emit fileDownloaded(filename, OperationResult::fileSystemError, QString());
                return;
            }

            if (!file.commit())
            {
                emit fileDownloaded(filename, OperationResult::fileSystemError, QString());
                return;
            }

            emit fileDownloaded(filename, OperationResult::ok, safeFilePath);
        },
        this);
    m_loading.insert(safeFilename, handle);
}

void ServerFileCache::uploadFile(const QString& unsafeFilename)
{
    const auto api = connectedServerApi();
    if (!api)
    {
        emitQueued(&ServerFileCache::fileUploaded,
            unsafeFilename, OperationResult::disconnected);
        return;
    }

    const auto safeFilename = file_cache::sanitizeFilename(unsafeFilename);
    if (safeFilename.isEmpty())
    {
        NX_WARNING(this, "Rejecting unsafe filename: %1", unsafeFilename);
        emitQueued(&ServerFileCache::fileUploaded,
            unsafeFilename, OperationResult::invalidOperation);
        return;
    }

    if (m_uploading.contains(safeFilename))
        return;

    const auto safeFilePath = absoluteFilePath(safeFilename);
    if (!NX_ASSERT(!safeFilePath.isEmpty(),
        "Sanitized file name should always produce a path: %1", unsafeFilename))
    {
        return;
    }

    QFile file(safeFilePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        emitQueued(&ServerFileCache::fileUploaded,
            safeFilename, OperationResult::fileSystemError);
        return;
    }

    api::StoredFileData storedFileData;
    storedFileData.path = relativeFilePath(safeFilename);
    storedFileData.data = file.readAll();
    file.close();

    const auto handle = api->putStoredFile(
        storedFileData,
        [this, filename = safeFilename](
            bool success,
            rest::Handle requestId,
            const rest::ErrorOrData<nx::vms::api::StoredFileData>& result)
        {
            const auto it = m_uploading.find(filename);
            if (it == m_uploading.end() || it.value() != requestId)
                return;
            m_uploading.erase(it);

            if (!isConnectedToServer())
            {
                emit fileUploaded(filename, OperationResult::disconnected);
                return;
            }

            if (!success || !result)
            {
                if (const auto failedPath = absoluteFilePath(filename); !failedPath.isEmpty())
                    QFile::remove(failedPath);
                else
                    NX_WARNING(this, "Rejecting unsafe filename: %1", filename);
                emit fileUploaded(filename, OperationResult::serverError);
                return;
            }

            emit fileUploaded(filename, OperationResult::ok);
        },
        this);

    m_uploading.insert(safeFilename, handle);
}

void ServerFileCache::deleteFile(const QString& unsafeFilename)
{
    const auto api = connectedServerApi();
    if (!api)
    {
        emitQueued(&ServerFileCache::fileDeleted,
            unsafeFilename, OperationResult::disconnected);
        return;
    }

    const auto safeFilename = file_cache::sanitizeFilename(unsafeFilename);
    if (safeFilename.isEmpty())
    {
        NX_WARNING(this, "Rejecting unsafe filename: %1", unsafeFilename);
        emitQueued(&ServerFileCache::fileDeleted,
            unsafeFilename, OperationResult::invalidOperation);
        return;
    }

    if (m_loading.remove(safeFilename))
    {
        // Notify subscribers that the pending download will not produce a result. The REST
        // request itself is not cancelled; its late callback will hit the m_loading guard and
        // discard the data.
        emitQueued(&ServerFileCache::fileDownloaded,
            safeFilename, OperationResult::invalidOperation, QString());
    }

    if (m_deleting.contains(safeFilename))
        return;

    const auto handle = api->deleteStoredFile(
        relativeFilePath(safeFilename),
        [this, filename = safeFilename](
            bool success,
            rest::Handle requestId,
            const rest::ServerConnection::ErrorOrEmpty& /*result*/)
        {
            const auto it = m_deleting.find(filename);
            if (it == m_deleting.end() || it.value() != requestId)
                return;
            m_deleting.erase(it);

            if (!isConnectedToServer())
            {
                emit fileDeleted(filename, OperationResult::disconnected);
                return;
            }

            if (success)
            {
                if (const auto removedPath = absoluteFilePath(filename); !removedPath.isEmpty())
                    QFile::remove(removedPath);
                else
                    NX_WARNING(this, "Rejecting unsafe filename: %1", filename);
            }

            emit fileDeleted(
                filename, success ? OperationResult::ok : OperationResult::serverError);
        },
        this);

    m_deleting.insert(safeFilename, handle);
}

void ServerFileCache::clear()
{
    m_loading.clear();
    m_uploading.clear();
    m_deleting.clear();
}

} // namespace nx::vms::client::desktop
