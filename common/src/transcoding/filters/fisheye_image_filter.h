#ifndef __FISHEYE_IMAGE_FILTER_H__
#define __FISHEYE_IMAGE_FILTER_H__

#include "abstract_filter.h"
#include "fisheye/fisheye_common.h"

class QnFisheyeImageFilter: public QnAbstractImageFilter
{
public:
    QnFisheyeImageFilter(const DewarpingParams& params);

    virtual void updateImage(CLVideoDecoderOutput* frame, const QRectF& updateRect) override;
private:
    void updateFisheyeTransform(const QSize& imageSize, int plane);
    void updateFisheyeTransformRectilinear(const QSize& imageSize, int plane);
    void updateFisheyeTransformEquirectangular(const QSize& imageSize, int plane);
private:
    static const int MAX_COLOR_PLANES = 4;

    DewarpingParams m_params;
    QSize m_lastImageSize;
    QPointF* m_transform[MAX_COLOR_PLANES];
    CLVideoDecoderOutput m_tmpBuffer;
    int m_lastImageFormat;
};

#endif // __FISHEYE_IMAGE_FILTER_H__
