
#pragma once

#include <transcoding/filters/abstract_image_filter.h>

class QImage;

class QnPaintImageFilter: public QnAbstractImageFilter
{
public:
    QnPaintImageFilter();

    virtual ~QnPaintImageFilter();

    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;
    virtual QSize updatedResolution(const QSize& sourceSize) override;

    void setImage(
        const QImage& image,
        const QPoint& position);

private:
    class Internal;
    const QScopedPointer<Internal> d;
};
