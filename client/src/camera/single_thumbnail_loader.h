#ifndef SINGLE_THUMBNAIL_LOADER_H
#define SINGLE_THUMBNAIL_LOADER_H

#include <QtCore/QObject>

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <utils/image_provider.h>

class QnSingleThumbnailLoader : public QnImageProvider {
    Q_OBJECT

    typedef QnImageProvider base_type;

public:
    enum ThumbnailFormat {
        PngFormat,
        JpgFormat
    };

    explicit QnSingleThumbnailLoader(const QnVirtualCameraResourcePtr &camera,
                                     const QnMediaServerResourcePtr &server,
                                     qint64 msecSinceEpoch,
                                     int rotation = -1,
                                     const QSize &size = QSize(),
                                     ThumbnailFormat format = JpgFormat,
                                     QObject *parent = NULL);

    virtual QImage image() const override;
protected:
    virtual void doLoadAsync() override;

    QString formatToString(ThumbnailFormat format);
private slots:
    void at_replyReceived(int status, const QImage& image, int requestHandle);

private:
    /** Camera that this loader gets thumbnail for. */
    QnVirtualCameraResourcePtr m_camera;

    /** Server that this loader uses. */
    QnMediaServerResourcePtr m_server;

    QImage m_image;

    /** Time in milliseconds since epoch */
    qint64 m_msecSinceEpoch;
    int m_rotation;
    QSize m_size;

    ThumbnailFormat m_format;
};

#endif // SINGLE_THUMBNAIL_LOADER_H
