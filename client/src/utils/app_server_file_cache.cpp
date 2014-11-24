#include "app_server_file_cache.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <utils/common/uuid.h>
#include <QtCore/QTimer>
#include <QtCore/QStandardPaths>

#include <utils/common/util.h> /* For removeDir. */

#include <api/app_server_connection.h>


QnAppServerFileCache::QnAppServerFileCache(QString folderName, QObject *parent) :
    QObject(parent),
    m_folderName(folderName)
{
    connect(this, SIGNAL(delayedFileDownloaded(QString,bool)),  this, SIGNAL(fileDownloaded(QString,bool)), Qt::QueuedConnection);
    connect(this, SIGNAL(delayedFileUploaded(QString,bool)),    this, SIGNAL(fileUploaded(QString,bool)), Qt::QueuedConnection);
    connect(this, SIGNAL(delayedFileDeleted(QString,bool)),     this, SIGNAL(fileDeleted(QString,bool)), Qt::QueuedConnection);
}

QnAppServerFileCache::~QnAppServerFileCache(){}

// -------------- Utility methods ----------------

QString QnAppServerFileCache::getFullPath(const QString &filename) const {
    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    QUrl url = QnAppServerConnectionFactory::url();
    return QDir::toNativeSeparators(QString(QLatin1String("%1/cache/%2_%3/%4/%5"))
                                    .arg(path)
                                    .arg(url.host(QUrl::FullyEncoded))
                                    .arg(url.port())
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
    QString dir = QDir::toNativeSeparators(QString(QLatin1String("%1/cache/")).arg(path));
    removeDir(dir);
}


// -------------- File List loading methods -----

void QnAppServerFileCache::getFileList() {
    auto connection = QnAppServerConnectionFactory::getConnection2();
    if (!connection)
        return;

    connection->getStoredFileManager()->listDirectory(m_folderName, this, [this](int handle, ec2::ErrorCode errorCode, const QStringList& filenames) {
        Q_UNUSED(handle);
        emit fileListReceived(filenames, errorCode == ec2::ErrorCode::ok);
    } );
}

// -------------- Download File methods ----------

void QnAppServerFileCache::downloadFile(const QString &filename) {
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

    auto connection = QnAppServerConnectionFactory::getConnection2();
    if (!connection)
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

    if (errorCode != ec2::ErrorCode::ok) {
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
    if (m_uploading.values().contains(filename))
        return;

    QFile file(getFullPath(filename));
    if(!file.open(QIODevice::ReadOnly)) {
        emit delayedFileUploaded(filename, false);
        return;
    }

    auto connection = QnAppServerConnectionFactory::getConnection2();
    if (!connection)
        return;

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
    const bool ok = errorCode == ec2::ErrorCode::ok;
    if (!ok)
        QFile::remove(getFullPath(filename));
    emit fileUploaded(filename, ok);
}

// -------------- Deleting methods ----------------

void QnAppServerFileCache::deleteFile(const QString &filename) {
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

    auto connection = QnAppServerConnectionFactory::getConnection2();
    if (!connection)
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
