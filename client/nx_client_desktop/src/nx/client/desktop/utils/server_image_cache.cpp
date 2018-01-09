#include "server_image_cache.h"

#include <QtCore/QFileInfo>

#include <ui/graphics/opengl/gl_functions.h>

#include <utils/common/id.h>
#include <nx/utils/uuid.h>
#include <nx/client/desktop/image_providers/threaded_image_loader.h>

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

void ServerImageCache::storeImage(const QString &filePath, const QnAspectRatio& aspectRatio)
{
    QString newFilename = cachedImageFilename(filePath);

    ensureCacheFolder();

    auto loader = new ThreadedImageLoader(this);
    loader->setInput(filePath);
    loader->setSize(getMaxImageSize());
    loader->setAspectRatio(aspectRatio);
    loader->setOutput(getFullPath(newFilename));
    connect(loader, &ThreadedImageLoader::imageSaved, this, &ServerImageCache::at_imageConverted);
    connect(loader, &ThreadedImageLoader::imageSaved, loader, &QObject::deleteLater);
    loader->start();
}

void ServerImageCache::at_imageConverted(const QString &filePath) {
    uploadFile(QFileInfo(filePath).fileName());
}

} // namespace desktop
} // namespace client
} // namespace nx
