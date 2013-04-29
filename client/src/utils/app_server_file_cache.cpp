#include "app_server_file_cache.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QUuid>

#include <QtGui/QDesktopServices>

#include <api/app_server_connection.h>


QnAppServerFileCache::QnAppServerFileCache(QObject *parent) :
    QObject(parent)
{}

QnAppServerFileCache::~QnAppServerFileCache(){}

// -------------- Utility methods ----------------

QString QnAppServerFileCache::getFullPath(const QString &filename) const {
    QString path = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    QUrl url = QnAppServerConnectionFactory::defaultUrl();
    return QDir::toNativeSeparators(QString(QLatin1String("%1/cache/%2_%3/%4"))
                                    .arg(path)
                                    .arg(QLatin1String(url.encodedHost()))
                                    .arg(url.port())
                                    .arg(filename)
                                    );
}

void QnAppServerFileCache::uploadFile(const QString &filename) {
    QFile file(filename);
    if(!file.open(QIODevice::ReadOnly)) {
        emit fileUploaded(filename, false);
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    if (m_uploading.values().contains(filename))
        return;

    int handle = QnAppServerConnectionFactory::createConnection()->addStoredFileAsync(
                filename,
                data,
                this,
                SLOT(at_fileUploaded(int, int))
                );
    m_uploading.insert(handle, filename);
}

// -------------- Loading image methods ----------------

void QnAppServerFileCache::downloadFile(const QString &filename) {
    if (filename.isEmpty()) {
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
                filename,
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

// -------------- Uploading image methods ----------------


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

