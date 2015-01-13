#include "app_server_file_cache.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>
#include <QtCore/QStandardPaths>

#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <utils/common/util.h> /* For removeDir. */
#include <utils/common/uuid.h>
#include <utils/common/string.h>

QnAppServerFileCache::QnAppServerFileCache(const QString &folderName, QObject *parent) :
    QObject(parent),
    m_folderName(folderName)
{
    connect(this, &QnAppServerFileCache::delayedFileDownloaded,     this,   [this](const QString &filename, bool ok) {
        auto connection = QnAppServerConnectionFactory::getConnection2();
        emit fileDownloaded(filename, ok && static_cast<bool>(connection));
    }, Qt::QueuedConnection);
    connect(this, &QnAppServerFileCache::delayedFileUploaded,     this,   [this](const QString &filename, bool ok) {
        auto connection = QnAppServerConnectionFactory::getConnection2();
        emit fileUploaded(filename, ok && static_cast<bool>(connection));
    }, Qt::QueuedConnection);
    connect(this, &QnAppServerFileCache::delayedFileDeleted,     this,   [this](const QString &filename, bool ok) {
        auto connection = QnAppServerConnectionFactory::getConnection2();
        emit fileDeleted(filename, ok && static_cast<bool>(connection));
    }, Qt::QueuedConnection);
    connect(this, &QnAppServerFileCache::delayedFileListReceived,     this,   [this](const QStringList &files, bool ok) {
        auto connection = QnAppServerConnectionFactory::getConnection2();
        emit fileListReceived(files, ok && static_cast<bool>(connection));
    }, Qt::QueuedConnection);
}

QnAppServerFileCache::~QnAppServerFileCache(){}

// -------------- Utility methods ----------------

QString QnAppServerFileCache::getFullPath(const QString &filename) const {
    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    QString systemName = replaceNonFileNameCharacters(qnCommon->localSystemName(), L'_');
    Q_ASSERT_X(!systemName.isEmpty(), Q_FUNC_INFO, "Method should be called only when we are connected to server");
    return QDir::toNativeSeparators(QString(lit("%1/cache/%2/%3/%4"))
                                    .arg(path)
                                    .arg(systemName)
                                    .arg(m_folderName)
                                    .arg(filename)
                                    );
}

void QnAppServerFileCache::ensureCacheFolder() {
    QString folderPath = getFullPath(QString());
    QDir().mkpath(folderPath);
}

QString QnAppServerFileCache::folderName() const {
    return m_folderName;
}

void QnAppServerFileCache::clearLocalCache() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    QString dir = QDir::toNativeSeparators(QString(lit("%1/cache/")).arg(path));
    removeDir(dir);
}

bool QnAppServerFileCache::isConnectedToServer() const {
    return QnAppServerConnectionFactory::getConnection2() != NULL;
}

// -------------- File List loading methods -----

void QnAppServerFileCache::getFileList() {
    auto connection = QnAppServerConnectionFactory::getConnection2();
    if (!connection) {
        emit delayedFileListReceived(QStringList(), false);
        return;
    }

    connection->getStoredFileManager()->listDirectory(m_folderName, this, [this](int handle, ec2::ErrorCode errorCode, const QStringList& filenames) {
        Q_UNUSED(handle);
        emit fileListReceived(filenames, errorCode == ec2::ErrorCode::ok);
    } );
}

// -------------- Download File methods ----------

void QnAppServerFileCache::downloadFile(const QString &filename) {
    auto connection = QnAppServerConnectionFactory::getConnection2();
    if (!connection) {
        emit delayedFileDownloaded(filename, false);
        return;
    }

    if (filename.isEmpty()) {
        emit delayedFileDownloaded(filename, false);
        return;
    }

    if (m_deleting.values().contains(filename)) {
        emit delayedFileDownloaded(filename, false);
        return;
    }

    QFileInfo info(getFullPath(filename));
    if (info.exists()) {
        emit delayedFileDownloaded(filename, true);
        return;
    }

    if (m_loading.values().contains(filename))
      return;

    int handle = connection->getStoredFileManager()->getStoredFile(
                m_folderName + QLatin1Char('/') + filename,
                this,
                &QnAppServerFileCache::at_fileLoaded );
    m_loading.insert(handle, filename);
}

void QnAppServerFileCache::at_fileLoaded( int handle, ec2::ErrorCode errorCode, const QByteArray& data ) {
    if (!m_loading.contains(handle))
        return;

    QString filename = m_loading[handle];
    m_loading.remove(handle);

    if (errorCode != ec2::ErrorCode::ok || !isConnectedToServer()) {
        emit fileDownloaded(filename, false);
        return;
    }

    ensureCacheFolder();
    QString filePath = getFullPath(filename);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit fileDownloaded(filename, false);
        return;
    }
    QDataStream out(&file);
    out.writeRawData(data, data.size());
    file.close();

    emit fileDownloaded(filename, true);
}

// -------------- Uploading methods ----------------


void QnAppServerFileCache::uploadFile(const QString &filename) {
    auto connection = QnAppServerConnectionFactory::getConnection2();
    if (!connection) {
        emit delayedFileUploaded(filename, false);
        return;
    }

    if (m_uploading.values().contains(filename))
        return;

    QFile file(getFullPath(filename));
    if(!file.open(QIODevice::ReadOnly)) {
        emit delayedFileUploaded(filename, false);
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    int handle = connection->getStoredFileManager()->addStoredFile(
                m_folderName + QLatin1Char('/') +filename,
                data,
                this,
                &QnAppServerFileCache::at_fileUploaded );
    m_uploading.insert(handle, filename);
}


void QnAppServerFileCache::at_fileUploaded( int handle, ec2::ErrorCode errorCode ) {
    if (!m_uploading.contains(handle))
        return;

    QString filename = m_uploading[handle];
    m_uploading.remove(handle);

    if (!isConnectedToServer()) {
        emit fileUploaded(filename, false);
        return;
    }

    const bool ok = errorCode == ec2::ErrorCode::ok;
    if (!ok)
        QFile::remove(getFullPath(filename));
    emit fileUploaded(filename, ok);
}

// -------------- Deleting methods ----------------

void QnAppServerFileCache::deleteFile(const QString &filename) {
    auto connection = QnAppServerConnectionFactory::getConnection2();
    if (!connection) {
        emit delayedFileDeleted(filename, false);
        return;
    }

    if (filename.isEmpty()) {
        emit delayedFileDeleted(filename, false);
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

    int handle = connection->getStoredFileManager()->deleteStoredFile(
                    m_folderName + QLatin1Char('/') +filename,
                    this,
                    &QnAppServerFileCache::at_fileDeleted );

    m_deleting.insert(handle, filename);
}

void QnAppServerFileCache::at_fileDeleted( int handle, ec2::ErrorCode errorCode ) {
    if (!m_deleting.contains(handle))
        return;

    if (!isConnectedToServer())
        return;

    QString filename = m_deleting[handle];
    m_deleting.remove(handle);
    const bool ok = errorCode == ec2::ErrorCode::ok;
    if (ok)
        QFile::remove(getFullPath(filename));
    emit fileDeleted(filename, ok);
}

void QnAppServerFileCache::clear() {
    m_loading.clear();
    m_uploading.clear();
    m_deleting.clear();
}
