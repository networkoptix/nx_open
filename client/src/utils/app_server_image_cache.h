#ifndef APP_SERVER_IMAGE_CACHE_H
#define APP_SERVER_IMAGE_CACHE_H

#include <utils/app_server_file_cache.h>

#include <QtGui/QImage>

class QnAppServerImageCache : public QnAppServerFileCache
{
    Q_OBJECT

    typedef QnAppServerFileCache base_type;
public:
    explicit QnAppServerImageCache(QObject *parent = 0);
    virtual ~QnAppServerImageCache();

    QSize getMaxImageSize() const;

    QString cachedImageFilename(const QString &sourcePath) const;

    void storeImage(const QString &filePath, const qreal targetAspectRatio = 0.0);
private slots:
    void at_imageConverted(const QString &filePath);

};

#endif // APP_SERVER_IMAGE_CACHE_H
