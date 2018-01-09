#pragma once

#include <core/resource/resource_fwd.h>

#include "image_provider.h"

/** Utility class to get an image from the local media file using ffmpeg. */
class QnFfmpegImageProvider: public QnImageProvider
{
    Q_OBJECT

    using base_type = QnImageProvider ;
public:
    QnFfmpegImageProvider(const QnResourcePtr &resource, QObject* parent = NULL);

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;
signals:
    void loadDelayed(const QImage &image);
protected:
    virtual void doLoadAsync() override;

private:
    QnResourcePtr m_resource;
    QImage m_image;
};
