#include "app_server_file_cache.h"

#include <QDesktopServices>
#include <QFileInfo>

#include <api/app_server_connection.h>

#include <ui/graphics/opengl/gl_functions.h>

#include <utils/threaded_image_loader.h>

QnAppServerFileCache::QnAppServerFileCache(QObject *parent) :
    QObject(parent)
{}

QnAppServerFileCache::~QnAppServerFileCache(){}


void QnAppServerFileCache::loadImage(int id) {
    if (id <= 0)
        return;

    QFileInfo info(getPath(id));
    if (info.exists()) {
        emit imageLoaded(id);
        return;
    }

    if (m_loading.values().contains(id))
      return;

    int handle = QnAppServerConnectionFactory::createConnection()->requestStoredFileAsync(
                id,
                this,
                SLOT(at_fileLoaded(int handle, const QByteArray &data))
                );
    m_loading.insert(handle, id);
}

void QnAppServerFileCache::storeImage(const QString &filename) {
    /*int handle = QnAppServerConnectionFactory::createConnection()->addStoredFileAsync(
                id,
                this,
                SLOT(at_fileUploaded(int handle, int id))
                );*/

    int i = 1;
    while (QFileInfo(getPath(i)).exists())
        i++;

    int maxTextureSize = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);
    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(filename);
    loader->setSize(QSize(maxTextureSize, maxTextureSize));
    loader->setOutput(getPath(i));
    connect(loader, SIGNAL(finished(QImage)), this, SLOT(saveImageDebug(QImage)));
    loader->start();

}

QString QnAppServerFileCache::getFolder() const {
    QString path = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    QUrl url = QnAppServerConnectionFactory::defaultUrl();
    return QDir::toNativeSeparators(QString(QLatin1String("%1/cache/%2_%3/"))
        .arg(path)
        .arg(QLatin1String(url.encodedHost()))
        .arg(url.port())
        );
}

QString QnAppServerFileCache::getPath(int id) const {
    return getFolder() + QString(QLatin1String("%4.png")).arg(id);
}

void QnAppServerFileCache::at_fileLoaded(int handle, const QByteArray &data) {
    if (!m_loading.contains(handle))
        return;

    int id = m_loading[handle];
    m_loading.remove(handle);

    QFile file(getPath(id));
    file.open(QIODevice::WriteOnly);
    QDataStream out(&file);
    out.writeRawData(data, data.size());
    emit imageLoaded(id);
}

void QnAppServerFileCache::saveImageDebug(const QImage &image) {
    int i = 1;
    while (QFileInfo(getPath(i)).exists())
        i++;
    emit imageStored(i-1);
}



