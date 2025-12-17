// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_file_cache.h"

#include <QtCore/QDataStream>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <nx/utils/string.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/stored_file_data.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/remote_session.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/util.h> //< For removeDir().

namespace nx::vms::client::desktop {

ServerFileCache::ServerFileCache(
    SystemContext* systemContext,
    const QString& folderName,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext),
    m_folderName(folderName)
{
    connect(this, &ServerFileCache::delayedFileDownloaded, this,
        [this](const QString &filename, OperationResult status)
        {
            if (isConnectedToServer())
                emit fileDownloaded(filename, status);
            else
                emit fileDownloaded(filename, OperationResult::disconnected);
        }, Qt::QueuedConnection);

    connect(this, &ServerFileCache::delayedFileUploaded, this,
        [this](const QString &filename, OperationResult status)
        {
            if (isConnectedToServer())
                emit fileUploaded(filename, status);
            else
                emit fileUploaded(filename, OperationResult::disconnected);
        }, Qt::QueuedConnection);

    connect(this, &ServerFileCache::delayedFileDeleted, this,
        [this](const QString &filename, OperationResult status)
        {
            if (isConnectedToServer())
                emit fileDeleted(filename, status);
            else
                emit fileDeleted(filename, OperationResult::disconnected);
        }, Qt::QueuedConnection);

    connect(this, &ServerFileCache::delayedFileListReceived, this,
        [this](const QStringList &files, OperationResult status)
        {
            if (isConnectedToServer())
                emit fileListReceived(files, status);
            else
                emit fileListReceived(files, OperationResult::disconnected);
        }, Qt::QueuedConnection);
}

ServerFileCache::~ServerFileCache() {
}

// -------------- Utility methods ----------------

QString ServerFileCache::getFullPath(const QString &filename) const {
    /* Avoid empty folder name and collisions with our folders such as 'log'. */
    const auto systemId = globalSettings()->localSystemId();
    //NX_ASSERT(!systemId.isNull());
    QString systemName =
        '_' + nx::utils::replaceNonFileNameCharacters(systemId.toString(QUuid::WithBraces), '_');

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir::toNativeSeparators(QString(lit("%1/cache/%2/%3/%4"))
                                    .arg(path)
                                    .arg(systemName)
                                    .arg(m_folderName)
                                    .arg(filename)
                                    );
}

void ServerFileCache::ensureCacheFolder() {
    QString folderPath = getFullPath(QString());
    QDir().mkpath(folderPath);
}

QString ServerFileCache::folderName() const {
    return m_folderName;
}

QString ServerFileCache::relativeFilePath(const QString& filename) const
{
    return nx::format("%1/%2", m_folderName, filename);
}

void ServerFileCache::clearLocalCache()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    if (dir.cd("cache"))
        dir.removeRecursively();
}

bool ServerFileCache::isConnectedToServer() const
{
    return connection().get() != nullptr;
}

// -------------- File List loading methods -----

void ServerFileCache::getFileList()
{
    const auto api = connectedServerApi();
    if (!api)
    {
        emit delayedFileListReceived(QStringList(), OperationResult::disconnected);
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
            for (const auto& file: *result)
                filenames << QFileInfo(file.path).fileName();

            emit fileListReceived(filenames, OperationResult::ok);
        },
        this);
}

// -------------- Download File methods ----------

void ServerFileCache::downloadFile(const QString& filename)
{
    const auto api = connectedServerApi();
    if (!api)
    {
        emit delayedFileDownloaded(filename, OperationResult::disconnected);
        return;
    }

    if (filename.isEmpty() || m_deleting.values().contains(filename))
    {
        emit delayedFileDownloaded(filename, OperationResult::invalidOperation);
        return;
    }

    QFileInfo info(getFullPath(filename));
    if (info.exists())
    {
        emit delayedFileDownloaded(filename, OperationResult::ok);
        return;
    }

    if (m_loading.values().contains(filename))
        return;

    const auto handle = api->getStoredFile(
        relativeFilePath(filename),
        [this](bool success,
            rest::Handle requestId,
            const rest::ErrorOrData<nx::vms::api::StoredFileData>& result)
        {
            if (!m_loading.contains(requestId))
                return;

            const auto filename = m_loading[requestId];
            m_loading.remove(requestId);

            if (!isConnectedToServer())
            {
                emit fileDownloaded(filename, OperationResult::disconnected);
                return;
            }

            if (!success || !result || result->data.size() <= 0)
            {
                emit fileDownloaded(filename, OperationResult::serverError);
                return;
            }

            ensureCacheFolder();
            const auto filePath = getFullPath(filename);
            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly))
            {
                emit fileDownloaded(filename, OperationResult::fileSystemError);
                return;
            }

            QDataStream out(&file);
            out.writeRawData(result->data, result->data.size());
            file.close();
            emit fileDownloaded(filename, OperationResult::ok);
        },
        this);
    m_loading.insert(handle, filename);
}

// -------------- Uploading methods ----------------

void ServerFileCache::uploadFile(const QString& filename)
{
    const auto api = connectedServerApi();
    if (!api)
    {
        emit delayedFileUploaded(filename, OperationResult::disconnected);
        return;
    }

    if (m_uploading.values().contains(filename))
        return;

    QFile file(getFullPath(filename));
    if (!file.open(QIODevice::ReadOnly))
    {
        emit delayedFileUploaded(filename, OperationResult::fileSystemError);
        return;
    }

    api::StoredFileData storedFileData;
    storedFileData.path = relativeFilePath(filename);
    storedFileData.data = file.readAll();
    file.close();

    const auto handle = api->putStoredFile(
        storedFileData,
        [this](bool success,
            rest::Handle requestId,
            const rest::ErrorOrData<nx::vms::api::StoredFileData>& result)
        {
            if (!m_uploading.contains(requestId))
                return;

            const auto filename = m_uploading[requestId];
            m_uploading.remove(requestId);

            if (!isConnectedToServer())
            {
                emit fileUploaded(filename, OperationResult::disconnected);
                return;
            }

            if (!success || !result)
            {
                QFile::remove(getFullPath(filename));
                emit fileUploaded(filename, OperationResult::serverError);
                return;
            }

            emit fileUploaded(filename, OperationResult::ok);
        },
        this);

    m_uploading.insert(handle, filename);
}

// -------------- Deleting methods ----------------

void ServerFileCache::deleteFile(const QString& filename)
{
    auto api = connectedServerApi();
    if (!api)
    {
        emit delayedFileDeleted(filename, OperationResult::disconnected);
        return;
    }

    if (filename.isEmpty())
    {
        emit delayedFileDeleted(filename, OperationResult::invalidOperation);
        return;
    }

    if (m_loading.values().contains(filename))
    {
        QHash<int, QString>::iterator i = m_loading.begin();
        while (i != m_loading.end())
        {
            QHash<int, QString>::iterator prev = i;
            ++i;
            if (prev.value() == filename)
                m_loading.erase(prev);
        }
    }

    if (m_deleting.values().contains(filename))
        return;

    const auto handle = api->deleteStoredFile(
        relativeFilePath(filename),
        [this](bool success,
            rest::Handle requestId,
            const rest::ServerConnection::ErrorOrEmpty& /*result*/)
        {
            if (!m_deleting.contains(requestId))
                return;

            QString filename = m_deleting[requestId];
            m_deleting.remove(requestId);

            if (!isConnectedToServer())
            {
                emit fileDeleted(filename, OperationResult::disconnected);
                return;
            }

            if (success)
                QFile::remove(getFullPath(filename));

            emit fileDeleted(
                filename, success ? OperationResult::ok : OperationResult::serverError);
        },
        this);

    m_deleting.insert(handle, filename);
}

void ServerFileCache::clear()
{
    m_loading.clear();
    m_uploading.clear();
    m_deleting.clear();
}

} // namespace nx::vms::client::desktop
