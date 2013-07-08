#ifndef __FISHEYE_IMAGE_FILTER_H__
#define __FISHEYE_IMAGE_FILTER_H__

#include "abstract_filter.h"
#include "fisheye/fisheye_common.h"

class QnFisheyeImageFilter: public QnAbstractImageFilter
{
public:
    QnFisheyeImageFilter(const DevorpingParams& params);

    virtual void updateImage(CLVideoDecoderOutput* frame, const QRectF& updateRect) override;
private:
    void updateFisheyeTransform(const QSize& imageSize);
private:
    DevorpingParams m_params;
    QSize m_lastImageSize;
    QPointF* m_transform;
};

#endif // __FISHEYE_IMAGE_FILTER_H__
