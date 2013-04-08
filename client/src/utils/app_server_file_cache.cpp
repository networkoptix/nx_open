#include "app_server_file_cache.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QUuid>

#include <QtGui/QDesktopServices>

#include <api/app_server_connection.h>

#include <ui/graphics/opengl/gl_functions.h>

#include <utils/threaded_image_loader.h>

namespace {
    const int maxImageSize = 4096;
}

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

QSize QnAppServerFileCache::getMaxImageSize() const {
    int value = qMin(QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE), maxImageSize);
    return QSize(value, value);
}

// -------------- Loading image methods ----------------

void QnAppServerFileCache::loadImage(const QString &filename) {
    if (filename.isEmpty()) {
        emit imageLoaded(filename, false);
        return;
    }

    QFileInfo info(getFullPath(filename));
    if (info.exists()) {
        emit imageLoaded(filename, true);
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
        emit imageLoaded(filename, false);
        return;
    }

    QFile file(getFullPath(filename));
    if (!file.open(QIODevice::WriteOnly)) {
        emit imageLoaded(filename, false);
        return;
    }
    QDataStream out(&file);
    out.writeRawData(data, data.size());
    file.close();

    emit imageLoaded(filename, true);
}

// -------------- Uploading image methods ----------------

void QnAppServerFileCache::storeImage(const QString &filePath) {
    QString uuid =  QUuid::createUuid().toString();
    QString newFilename = uuid.mid(1, uuid.size() - 2) + QLatin1String(".png");

    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(filePath);
    loader->setSize(getMaxImageSize());
    loader->setOutput(getFullPath(newFilename));
    connect(loader, SIGNAL(finished(QString)), this, SLOT(at_imageConverted(QString)));
    loader->start();
}

void QnAppServerFileCache::at_imageConverted(const QString &filePath) {
    QString filename = QFileInfo(filePath).fileName();

    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly)) {
        emit imageStored(filename, false);
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


void QnAppServerFileCache::at_fileUploaded(int status, int handle) {
    if (!m_uploading.contains(handle))
        return;

    QString filename = m_uploading[handle];
    m_uploading.remove(handle);
    bool ok = status == 0;
    if (!ok)
        QFile::remove(getFullPath(filename));
    emit imageStored(filename, ok);
}

