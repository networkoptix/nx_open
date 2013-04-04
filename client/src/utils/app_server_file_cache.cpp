#include "app_server_file_cache.h"

#include <QDesktopServices>
#include <QFileInfo>

#include <api/app_server_connection.h>

#include <ui/graphics/opengl/gl_functions.h>

#include <utils/threaded_image_loader.h>

#include "version.h"

QnAppServerFileCache::QnAppServerFileCache(QObject *parent) :
    QObject(parent),
    m_uploadingHandle(0)
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
    int maxTextureSize = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);
    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(filename);
    loader->setSize(QSize(maxTextureSize, maxTextureSize));
    loader->setOutput(getUploadingPath());
    connect(loader, SIGNAL(finished(int)), this, SLOT(at_imageConverted(int)));
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

QString QnAppServerFileCache::getUploadingPath() const {
    QString path = QDesktopServices::storageLocation(QDesktopServices::TempLocation);
    return QDir::toNativeSeparators(QString(QLatin1String("%1/%2/uploading.png"))
                                    .arg(path)
                                    .arg(QLatin1String(QN_PRODUCT_NAME))
                                    );
}

void QnAppServerFileCache::at_imageConverted(int tag) {
    Q_UNUSED(tag)

    QFile file(getUploadingPath());
    if(!file.open(QIODevice::ReadOnly)) {
        //TODO: #GDM data from image?
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    m_uploadingHandle = QnAppServerConnectionFactory::createConnection()->addStoredFileAsync(
                data,
                this,
                SLOT(at_fileUploaded(int handle, int id))
                );
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

void QnAppServerFileCache::at_fileUploaded(int handle, int id) {
    if (m_uploadingHandle != handle)
        return;
    m_uploadingHandle = 0;

    int maxTextureSize = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);
    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(getUploadingPath());
    loader->setSize(QSize(maxTextureSize, maxTextureSize));
    loader->setOutput(getPath(id));
    loader->setTag(id);
    connect(loader, SIGNAL(finished(int)), this, SIGNAL(imageStored(int)));
    loader->start();
}

