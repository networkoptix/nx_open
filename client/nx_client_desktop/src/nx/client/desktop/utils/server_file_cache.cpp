#include "server_file_cache.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>
#include <QtCore/QStandardPaths>

#include <api/app_server_connection.h>

#include <common/common_module.h>
#include <client/client_message_processor.h>

#include <utils/common/util.h> /* For removeDir. */
#include <nx/utils/uuid.h>
#include <nx/utils/string.h>
#include <api/global_settings.h>
#include <network/system_helpers.h>

namespace {
/** Maximum allowed file size is 15 Mb, hard limit of Jaguar or S2 cameras. */
const qint64 maximumFileSize = 15 * 1024 * 1024;
}

namespace nx {
namespace client {
namespace desktop {

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
    QString systemName = L'_' + nx::utils::replaceNonFileNameCharacters(
        helpers::currentSystemLocalId(commonModule()).toString(), L'_');

    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
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

void ServerFileCache::clearLocalCache() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    QString dir = QDir::toNativeSeparators(QString(lit("%1/cache/")).arg(path));
    removeDir(dir);
}

bool ServerFileCache::isConnectedToServer() const {
    return commonModule()->ec2Connection() != NULL;
}

qint64 ServerFileCache::maximumFileSize() {
    return ::maximumFileSize;
}


// -------------- File List loading methods -----

void ServerFileCache::getFileList() {
    if (!isConnectedToServer()) {
        emit delayedFileListReceived(QStringList(), OperationResult::disconnected);
        return;
    }

    auto connection = commonModule()->ec2Connection();
    connection->getStoredFileManager(Qn::kSystemAccess)->listDirectory(m_folderName, this, [this](int handle, ec2::ErrorCode errorCode, const QStringList& filenames) {
        Q_UNUSED(handle);
        bool ok = errorCode == ec2::ErrorCode::ok;
        emit fileListReceived(filenames, ok ? OperationResult::ok : OperationResult::serverError);
    } );
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

    auto connection = commonModule()->ec2Connection();
    int handle = connection->getStoredFileManager(Qn::kSystemAccess)->getStoredFile(
                m_folderName + QLatin1Char('/') + filename,
                this,
                &ServerFileCache::at_fileLoaded );
    m_loading.insert(handle, filename);
}

void ServerFileCache::at_fileLoaded( int handle, ec2::ErrorCode errorCode, const QByteArray& data ) {
    if (!m_loading.contains(handle))
        return;

    QString filename = m_loading[handle];
    m_loading.remove(handle);

    if (!isConnectedToServer()) {
        emit fileDownloaded(filename, OperationResult::disconnected);
        return;
    }

    if (errorCode != ec2::ErrorCode::ok) {
        emit fileDownloaded(filename, OperationResult::serverError);
        return;
    }

    if (data.size() <= 0) {
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
    out.writeRawData(data, data.size());
    file.close();

    emit fileDownloaded(filename, OperationResult::ok);
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

    if(file.size() > ::maximumFileSize) {
        emit delayedFileUploaded(filename, OperationResult::sizeLimitExceeded);
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    auto connection = commonModule()->ec2Connection();
    int handle = connection->getStoredFileManager(Qn::kSystemAccess)->addStoredFile(
                m_folderName + QLatin1Char('/') +filename,
                data,
                this,
                &ServerFileCache::at_fileUploaded );
    m_uploading.insert(handle, filename);
}


void ServerFileCache::at_fileUploaded( int handle, ec2::ErrorCode errorCode ) {
    if (!m_uploading.contains(handle))
        return;

    QString filename = m_uploading[handle];
    m_uploading.remove(handle);

    if (!isConnectedToServer()) {
        emit fileUploaded(filename, OperationResult::disconnected);
        return;
    }

    const bool ok = errorCode == ec2::ErrorCode::ok;
    if (!ok)
        QFile::remove(getFullPath(filename));
    emit fileUploaded(filename, ok ? OperationResult::ok : OperationResult::serverError);
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

    auto connection = commonModule()->ec2Connection();
    int handle = connection->getStoredFileManager(Qn::kSystemAccess)->deleteStoredFile(
                    m_folderName + QLatin1Char('/') +filename,
                    this,
                    &ServerFileCache::at_fileDeleted );

    m_deleting.insert(handle, filename);
}

void ServerFileCache::at_fileDeleted( int handle, ec2::ErrorCode errorCode ) {
    if (!m_deleting.contains(handle))
        return;

    QString filename = m_deleting[handle];
    m_deleting.remove(handle);

    if (!isConnectedToServer()) {
        emit fileDeleted(filename, OperationResult::disconnected);
        return;
    }

    const bool ok = errorCode == ec2::ErrorCode::ok;
    if (ok)
        QFile::remove(getFullPath(filename));
    emit fileDeleted(filename, ok ? OperationResult::ok : OperationResult::serverError);
}

void ServerFileCache::clear() {
    m_loading.clear();
    m_uploading.clear();
    m_deleting.clear();
}

} // namespace desktop
} // namespace client
} // namespace nx
