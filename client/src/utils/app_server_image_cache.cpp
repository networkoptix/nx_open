#include "app_server_image_cache.h"

#include <QtCore/QFileInfo>

#include <ui/graphics/opengl/gl_functions.h>

#include <utils/common/id.h>
#include <utils/common/uuid.h>
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


QString QnAppServerImageCache::cachedImageFilename(const QString &sourcePath) {
    QString uuid = guidFromArbitraryData(sourcePath.toUtf8()).toString();
    return uuid.mid(1, uuid.size() - 2) + lit(".png");
}


void QnAppServerImageCache::storeImage(const QString &filePath, const qreal targetAspectRatio) {
    QString newFilename = cachedImageFilename(filePath);

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
