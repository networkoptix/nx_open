#include "server_image_cache.h"

#include <QtCore/QFileInfo>

#include <ui/graphics/opengl/gl_functions.h>

#include <utils/common/id.h>
#include <nx/utils/uuid.h>
#include <utils/threaded_image_loader.h>

namespace nx {
namespace client {
namespace desktop {

ServerImageCache::ServerImageCache(QObject *parent) :
    base_type(Qn::kWallpapersFolder, parent)
{
}

ServerImageCache::~ServerImageCache() {
}


QSize ServerImageCache::getMaxImageSize() const {
    int value = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);
    return QSize(value, value);
}


QString ServerImageCache::cachedImageFilename(const QString &sourcePath) {
    QString ext = QFileInfo(sourcePath).suffix();
    if (ext.isEmpty())
        ext = lit("png");

    QString uuid = guidFromArbitraryData(sourcePath.toUtf8()).toString();
    return uuid.mid(1, uuid.size() - 2) + L'.' + ext;
}

void ServerImageCache::storeImage(const QString &filePath, const qreal targetAspectRatio) {
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

void ServerImageCache::at_imageConverted(const QString &filePath) {
    uploadFile(QFileInfo(filePath).fileName());
}

} // namespace desktop
} // namespace client
} // namespace nx
