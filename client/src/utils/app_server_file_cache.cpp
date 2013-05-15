#include "app_server_file_cache.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QUuid>
#include <QtCore/QTimer>

#include <QtGui/QDesktopServices>

#include <api/app_server_connection.h>


QnAppServerFileCache::QnAppServerFileCache(QString folderName, QObject *parent) :
    QObject(parent),
    m_fileListHandle(0),
    m_folderName(folderName)
{}

QnAppServerFileCache::~QnAppServerFileCache(){}

// -------------- Utility methods ----------------

QString QnAppServerFileCache::getFullPath(const QString &filename) const {
    QString path = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    QUrl url = QnAppServerConnectionFactory::defaultUrl();
    return QDir::toNativeSeparators(QString(QLatin1String("%1/cache/%2_%3/%4/%5"))
                                    .arg(path)
                                    .arg(QLatin1String(url.encodedHost()))
                                    .arg(url.port())
                                    .arg(m_folderName)
                                    .arg(filename)
                                    );
}


// -------------- File List loading methods -----

void QnAppServerFileCache::getFileList() {
    m_fileListHandle = QnAppServerConnectionFactory::createConnection()->requestDirectoryListingAsync(
                m_folderName,
                this,
                SLOT(at_fileListReceived(int, const QStringList &, int))
                );
}

void QnAppServerFileCache::at_fileListReceived(int status, const QStringList &filenames, int handle) {
    if (handle != m_fileListHandle)
        return;

    emit fileListReceived(filenames, status == 0);
}

// -------------- Download File methods ----------

void QnAppServerFileCache::downloadFile(const QString &filename) {
    if (filename.isEmpty()) {
        emit fileDownloaded(filename, false);
        return;
    }

    if (m_deleting.values().contains(filename)) {
        emit fileDownloaded(filename, false);
        return;
    }

    QFileInfo info(getFullPath(filename));
    if (info.exists()) {
        emit fileDownloaded(filename, true);
        return;
    }

    if (m_loading.values().contains(filename))
      return;

    int handle = QnAppServerConnectionFactory::createConnection()->requestStoredFileAsync(
                m_folderName + QLatin1Char('/') + filename,
                this,
                SLOT(at_fileLoaded(int, const QByteArray&, int))
                );
    m_loading.insert(handle, filename);
}

void QnAppServerFileCache::at_fileLoaded(int status, const QByteArray& data, int handle) {
    if (!m_loading.contains(handle))
        return;

    QString filename = m_loading[handle];
    m_loading.remove(handle);

    if (status != 0) {
        emit fileDownloaded(filename, false);
        return;
    }

    QString filePath = getFullPath(filename);
    QString folder = QFileInfo(filePath).absolutePath();
    QDir().mkpath(folder);
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
    QFile file(getFullPath(filename));
    if(!file.open(QIODevice::ReadOnly)) {
        emit fileUploaded(filename, false);
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    if (m_uploading.values().contains(filename))
        return;

    int handle = QnAppServerConnectionFactory::createConnection()->addStoredFileAsync(
                m_folderName + QLatin1Char('/') +filename,
                data,
                this,
                SLOT(at_fileUploaded(int, int))
                );
    m_uploading.insert(handle, filename);
}


void QnAppServerFileCache::at_fileUploaded(int status, int handle) {
    if (!m_uploading.contains(handle))
        return;

    QString filename = m_uploading[handle];
    m_uploading.remove(handle);
    bool ok = status == 0;
    if (!ok)
        QFile::remove(getFullPath(filename));
    emit fileUploaded(filename, ok);
}

// -------------- Deleting methods ----------------

void QnAppServerFileCache::deleteFile(const QString &filename) {
    if (filename.isEmpty()) {
        emit fileDeleted(filename, false);
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

    int handle = QnAppServerConnectionFactory::createConnection()->deleteStoredFileAsync(
                    m_folderName + QLatin1Char('/') +filename,
                    this,
                    SLOT(at_fileDeleted(int, int))
                    );

    m_deleting.insert(handle, filename);
}

void QnAppServerFileCache::at_fileDeleted(int status, int handle) {
    if (!m_deleting.contains(handle))
        return;

    QString filename = m_deleting[handle];
    m_deleting.remove(handle);
    bool ok = status == 0;
    if (ok)
        QFile::remove(getFullPath(filename));
    emit fileDeleted(filename, ok);
}
