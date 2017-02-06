#ifndef __CROP_IMAGE_FILTER_H__
#define __CROP_IMAGE_FILTER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QScopedPointer>

#include "abstract_image_filter.h"

class QnCropImageFilter: public QnAbstractImageFilter
{
public:
    /**
     * Construct crop filter with fixed size
     */
    QnCropImageFilter(const QRect& rect);

    /**
     * Construct crop filter with dynamic size, measured in parts [0..1] of a source frame size
     */
    QnCropImageFilter(const QRectF& rect);

    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;

    virtual QSize updatedResolution(const QSize& srcSize) override;
private:
    QRectF m_rectF;
    QRect m_rect;
    QSize m_size;
    CLVideoDecoderOutputPtr m_tmpRef;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __CROP_IMAGE_FILTER_H__
