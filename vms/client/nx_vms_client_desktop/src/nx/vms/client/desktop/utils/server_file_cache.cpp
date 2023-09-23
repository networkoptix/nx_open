// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_file_cache.h"

#include <QtCore/QDataStream>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>

#include <client/client_message_processor.h>
#include <common/common_module.h>
#include <network/system_helpers.h>
#include <nx/utils/string.h>
#include <nx/utils/uuid.h>
#include <nx/vms/common/system_settings.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_stored_file_manager.h>
#include <utils/common/util.h> /* For removeDir. */

namespace nx::vms::client::desktop {

ServerFileCache::ServerFileCache(const QString &folderName, QObject *parent) :
    QObject(parent),
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
    NX_ASSERT(!systemId.isNull());
    QString systemName = '_' + nx::utils::replaceNonFileNameCharacters(systemId.toString(), '_');

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

void ServerFileCache::getFileList() {
    if (!isConnectedToServer()) {
        emit delayedFileListReceived(QStringList(), OperationResult::disconnected);
        return;
    }

    auto connection = RemoteConnectionAware::messageBusConnection();
    connection->getStoredFileManager(Qn::kSystemAccess)->listDirectory(
        m_folderName,
        [this](int /*requestId*/, ec2::ErrorCode errorCode, const QStringList& filenames)
        {
            bool ok = errorCode == ec2::ErrorCode::ok;
            emit fileListReceived(filenames, ok ? OperationResult::ok : OperationResult::serverError);
        },
        this);
}

// -------------- Download File methods ----------

void ServerFileCache::downloadFile(const QString &filename) {
    if (!isConnectedToServer()) {
        emit delayedFileDownloaded(filename, OperationResult::disconnected);
        return;
    }

    if (filename.isEmpty()) {
        emit delayedFileDownloaded(filename, OperationResult::invalidOperation);
        return;
    }

    if (m_deleting.values().contains(filename)) {
        emit delayedFileDownloaded(filename, OperationResult::invalidOperation);
        return;
    }

    QFileInfo info(getFullPath(filename));
    if (info.exists()) {
        emit delayedFileDownloaded(filename, OperationResult::ok);
        return;
    }

    if (m_loading.values().contains(filename))
      return;

    auto connection = RemoteConnectionAware::messageBusConnection();
    int handle = connection->getStoredFileManager(Qn::kSystemAccess)->getStoredFile(
        m_folderName + QLatin1Char('/') + filename,
        [this](int requestId, ec2::ErrorCode errorCode, const QByteArray& fileData)
        {
            if (!m_loading.contains(requestId))
                return;

            QString filename = m_loading[requestId];
            m_loading.remove(requestId);

            if (!isConnectedToServer()) {
                emit fileDownloaded(filename, OperationResult::disconnected);
                return;
            }

            if (errorCode != ec2::ErrorCode::ok) {
                emit fileDownloaded(filename, OperationResult::serverError);
                return;
            }

            if (fileData.size() <= 0) {
                emit fileDownloaded(filename, OperationResult::serverError);
                return;
            }

            ensureCacheFolder();
            QString filePath = getFullPath(filename);
            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly)) {
                emit fileDownloaded(filename, OperationResult::fileSystemError);
                return;
            }
            QDataStream out(&file);
            out.writeRawData(fileData, fileData.size());
            file.close();

            emit fileDownloaded(filename, OperationResult::ok);
        },
        this);
    m_loading.insert(handle, filename);
}

// -------------- Uploading methods ----------------

void ServerFileCache::uploadFile(const QString &filename) {
    if (!isConnectedToServer()) {
        emit delayedFileUploaded(filename, OperationResult::disconnected);
        return;
    }

    if (m_uploading.values().contains(filename))
        return;

    QFile file(getFullPath(filename));
    if(!file.open(QIODevice::ReadOnly)) {
        emit delayedFileUploaded(filename, OperationResult::fileSystemError);
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    auto connection = RemoteConnectionAware::messageBusConnection();
    int handle = connection->getStoredFileManager(Qn::kSystemAccess)->addStoredFile(
        m_folderName + QLatin1Char('/') + filename,
        data,
        [this](int requestId, ec2::ErrorCode errorCode)
        {
            if (!m_uploading.contains(requestId))
                return;

            QString filename = m_uploading[requestId];
            m_uploading.remove(requestId);

            if (!isConnectedToServer()) {
                emit fileUploaded(filename, OperationResult::disconnected);
                return;
            }

            const bool ok = errorCode == ec2::ErrorCode::ok;
            if (!ok)
                QFile::remove(getFullPath(filename));
            emit fileUploaded(filename, ok ? OperationResult::ok : OperationResult::serverError);
        },
        this);
    m_uploading.insert(handle, filename);
}

// -------------- Deleting methods ----------------

void ServerFileCache::deleteFile(const QString &filename) {
    if (!isConnectedToServer()) {
        emit delayedFileDeleted(filename, OperationResult::disconnected);
        return;
    }

    if (filename.isEmpty()) {
        emit delayedFileDeleted(filename, OperationResult::invalidOperation);
        return;
    }

    if (m_loading.values().contains(filename)) {
        QHash<int, QString>::iterator i = m_loading.begin();
        while (i != m_loading.end()) {
            QHash<int, QString>::iterator prev = i;
            ++i;
            if (prev.value() == filename)
                m_loading.erase(prev);
        }
    }

    if (m_deleting.values().contains(filename))
      return;

    auto connection = RemoteConnectionAware::messageBusConnection();
    int handle = connection->getStoredFileManager(Qn::kSystemAccess)->deleteStoredFile(
        m_folderName + QLatin1Char('/') + filename,
        [this](int requestId, ec2::ErrorCode errorCode)
        {
            if (!m_deleting.contains(requestId))
                return;

            QString filename = m_deleting[requestId];
            m_deleting.remove(requestId);

            if (!isConnectedToServer()) {
                emit fileDeleted(filename, OperationResult::disconnected);
                return;
            }

            const bool ok = errorCode == ec2::ErrorCode::ok;
            if (ok)
                QFile::remove(getFullPath(filename));
            emit fileDeleted(filename, ok ? OperationResult::ok : OperationResult::serverError);
        },
        this);

    m_deleting.insert(handle, filename);
}

void ServerFileCache::clear() {
    m_loading.clear();
    m_uploading.clear();
    m_deleting.clear();
}

} // namespace nx::vms::client::desktop
