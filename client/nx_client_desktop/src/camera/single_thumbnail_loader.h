#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <api/helpers/thumbnail_request_data.h>

#include <core/resource/resource_fwd.h>
#include <utils/image_provider.h>

class QnSingleThumbnailLoader: public QnImageProvider, public QnConnectionContextAware
{
    Q_OBJECT

    using base_type = QnImageProvider;

public:
    explicit QnSingleThumbnailLoader(const QnVirtualCameraResourcePtr &camera,
        qint64 msecSinceEpoch = QnThumbnailRequestData::kLatestThumbnail,
        int rotation = QnThumbnailRequestData::kDefaultRotation,
        const QSize &size = QSize(),
        QnThumbnailRequestData::ThumbnailFormat format = QnThumbnailRequestData::JpgFormat,
        QObject *parent = NULL);

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    QnThumbnailRequestData requestData() const;
    void setRequestData(const QnThumbnailRequestData& data);

signals:
    /** Internal signal to implement thread-safety. */
    void imageLoaded(const QByteArray &data);

protected:
    virtual void doLoadAsync() override;

private:
    void setStatus(Qn::ThumbnailStatus status);

private:
    QnThumbnailRequestData m_request;
    QImage m_image;
    Qn::ThumbnailStatus m_status;
};
