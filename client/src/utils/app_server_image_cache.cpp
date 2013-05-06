#include "app_server_image_cache.h"

#include <QtCore/QFileInfo>

#include <ui/graphics/opengl/gl_functions.h>

#include <utils/threaded_image_loader.h>

QnAppServerImageCache::QnAppServerImageCache(QObject *parent) :
    base_type(parent)
{
}

QnAppServerImageCache::~QnAppServerImageCache() {
}

QSize QnAppServerImageCache::getMaxImageSize() const {
    int value = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);
    return QSize(value, value);
}

void QnAppServerImageCache::storeImage(const QString &filePath, bool cropImageToMonitorAspectRatio) {
    QString uuid =  QUuid::createUuid().toString();
    QString newFilename = uuid.mid(1, uuid.size() - 2) + QLatin1String(".png");

    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(filePath);
    loader->setSize(getMaxImageSize());
    loader->setCropToMonitorAspectRatio(cropImageToMonitorAspectRatio);
    loader->setOutput(getFullPath(newFilename));
    connect(loader, SIGNAL(finished(QString)), this, SLOT(at_imageConverted(QString)));
    loader->start();
}


void QnAppServerImageCache::at_imageConverted(const QString &filePath) {
    uploadFile(QFileInfo(filePath).fileName());
}
