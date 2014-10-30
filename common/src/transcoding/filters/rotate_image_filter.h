#ifndef __ROTATE_IMAGE_FILTER_H__
#define __ROTATE_IMAGE_FILTER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QScopedPointer>

#include "abstract_image_filter.h"

class QnRotateImageFilter: public QnAbstractImageFilter
{
public:
    QnRotateImageFilter(int angle);

    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame, const QRectF& updateRect, qreal ar) override;

    static QSize getOptimalSize(const QSize& srcResolution);

private:
    int m_angle;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __ROTATE_IMAGE_FILTER_H__
