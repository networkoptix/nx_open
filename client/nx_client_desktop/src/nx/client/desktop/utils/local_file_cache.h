#pragma once

#include <QtCore/QObject>
#include <nx/client/desktop/utils/server_image_cache.h>

namespace nx {
namespace client {
namespace desktop {

class LocalFileCache: public ServerImageCache
{
    Q_OBJECT
    using base_type = ServerImageCache;
public:
    explicit LocalFileCache(QObject* parent = nullptr);
    virtual ~LocalFileCache() override;

    /** Get full path to cached file with fixed filename */
    virtual QString getFullPath(const QString& filename) const override;

    /**
     * @brief storeLocalImage
     * @param fileName
     * @param image
     */
    void storeImageData(const QString& fileName, const QByteArray& imageData);

    void storeImageData(const QString& fileName, const QImage& image);

    virtual void downloadFile(const QString& filename) override;
    virtual void uploadFile(const QString& filename) override;
};

} // namespace desktop
} // namespace client
} // namespace nx
