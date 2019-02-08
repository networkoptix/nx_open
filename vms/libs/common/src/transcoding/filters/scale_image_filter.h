#ifndef __SCALE_IMAGE_FILTER_H__
#define __SCALE_IMAGE_FILTER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QScopedPointer>

#include <libavutil/pixfmt.h>

#include "abstract_image_filter.h"

class QnScaleImageFilter: public QnAbstractImageFilter
{
public:
    QnScaleImageFilter(const QSize& size, AVPixelFormat format = AV_PIX_FMT_NONE);

    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;

    virtual QSize updatedResolution(const QSize& srcSize) override;

    void setOutputImageSize( const QSize& size );

private:
    QSize m_size;
    AVPixelFormat m_format;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __SCALE_IMAGE_FILTER_H__
