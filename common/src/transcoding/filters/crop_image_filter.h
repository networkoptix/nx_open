#ifndef __CROP_IMAGE_FILTER_H__
#define __CROP_IMAGE_FILTER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QScopedPointer>

#include "abstract_image_filter.h"

class QnCropImageFilter: public QnAbstractImageFilter
{
public:
    QnCropImageFilter(const QRect& rect);

    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;

    virtual QSize updatedResolution(const QSize& srcSize) override;
private:
    QRect m_rect;
    CLVideoDecoderOutputPtr m_tmpRef;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __CROP_IMAGE_FILTER_H__
