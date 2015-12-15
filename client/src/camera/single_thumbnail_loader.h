#pragma once

#include <QtCore/QObject>

#include <api/api_fwd.h>

#include <api/helpers/thumbnail_request_data.h>

#include <core/resource/resource_fwd.h>
#include <utils/image_provider.h>

class QnCameraThumbnailManager;

class QnSingleThumbnailLoader : public QnImageProvider {
    Q_OBJECT

    typedef QnImageProvider base_type;

public:
    explicit QnSingleThumbnailLoader(const QnVirtualCameraResourcePtr &camera,
                                     qint64 msecSinceEpoch = QnThumbnailRequestData::kLatestThumbnail,
                                     int rotation = QnThumbnailRequestData::kDefaultRotation,
                                     const QSize &size = QSize(),
                                     QnThumbnailRequestData::ThumbnailFormat format = QnThumbnailRequestData::JpgFormat,
                                     QSharedPointer<QnCameraThumbnailManager> statusPixmapManager = QSharedPointer<QnCameraThumbnailManager>(),
                                     QObject *parent = NULL);

    virtual QImage image() const override;

signals:
    /** Internal signal to implement thread-safety. */
    void imageLoaded(const QByteArray &data);

protected:
    virtual void doLoadAsync() override;

private:
    QnThumbnailRequestData m_request;
    QImage m_image;
};
