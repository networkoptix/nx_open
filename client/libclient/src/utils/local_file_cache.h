#ifndef LOCAL_FILE_CACHE_H
#define LOCAL_FILE_CACHE_H

#include <QtCore/QObject>
#include <utils/app_server_image_cache.h>

class QnLocalFileCache : public QnAppServerImageCache
{
    Q_OBJECT

    typedef QnAppServerImageCache base_type;
public:
    explicit QnLocalFileCache(QObject *parent = 0);
    virtual ~QnLocalFileCache();

    /** Get full path to cached file with fixed filename */
    virtual QString getFullPath(const QString &filename) const override;

    /**
     * @brief storeLocalImage
     * @param fileName
     * @param image
     */
    void storeImageData(const QString &fileName, const QByteArray &imageData);

    void storeImageData(const QString &fileName, const QImage &image);

    virtual void downloadFile(const QString &filename) override;
    virtual void uploadFile(const QString &filename) override;
};

#endif // LOCAL_FILE_CACHE_H
