#include "app_server_image_cache.h"

#include <QtCore/QFileInfo>
#include <utils/common/uuid.h>

#include <ui/graphics/opengl/gl_functions.h>

#include <utils/threaded_image_loader.h>

namespace {
    const QLatin1String folder("wallpapers");
}

QnAppServerImageCache::QnAppServerImageCache(QObject *parent) :
    base_type(folder, parent)
{
}

QnAppServerImageCache::~QnAppServerImageCache() {
}


QSize QnAppServerImageCache::getMaxImageSize() const {
    int value = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);
    return QSize(value, value);
}

void QnAppServerImageCache::storeImage(const QString &filePath, const qreal targetAspectRatio) {
    QString uuid = QnUuid::createUuid().toString();
    QString newFilename = uuid.mid(1, uuid.size() - 2) + QLatin1String(".png");

    ensureCacheFolder();

    QnThreadedImageLoader* loader = new QnThreadedImageLoader(this);
    loader->setInput(filePath);
    loader->setSize(getMaxImageSize());
    loader->setAspectRatio(targetAspectRatio);
    loader->setOutput(getFullPath(newFilename));
    connect(loader, SIGNAL(finished(QString)), this, SLOT(at_imageConverted(QString)));
    connect(loader, SIGNAL(finished(QString)), loader, SLOT(deleteLater()));
    loader->start();
}

void QnAppServerImageCache::at_imageConverted(const QString &filePath) {
    uploadFile(QFileInfo(filePath).fileName());
}
