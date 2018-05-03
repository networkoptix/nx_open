#pragma once

#include <nx/client/desktop/utils/server_file_cache.h>

#include <QtGui/QImage>
#include <utils/common/aspect_ratio.h>

namespace nx {
namespace client {
namespace desktop {

class ServerImageCache: public ServerFileCache
{
    Q_OBJECT

    typedef ServerFileCache base_type;
public:
    explicit ServerImageCache(QObject *parent = 0);
    virtual ~ServerImageCache();

    QSize getMaxImageSize() const;

    static QString cachedImageFilename(const QString &sourcePath);

    void storeImage(const QString &filePath, const QnAspectRatio& aspectRatio = QnAspectRatio());

private slots:
    void at_imageConverted(const QString &filePath);

};

} // namespace desktop
} // namespace client
} // namespace nx
