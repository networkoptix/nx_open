#ifndef __FISHEYE_IMAGE_FILTER_H__
#define __FISHEYE_IMAGE_FILTER_H__

#include "abstract_filter.h"

#include <core/ptz/media_dewarping_params.h>
#include <core/ptz/item_dewarping_params.h>

class QnFisheyeImageFilter: public QnAbstractImageFilter
{
public:
    QnFisheyeImageFilter(const QnMediaDewarpingParams& mediaDewarping,
                         const QnItemDewarpingParams& itemDewarping);

    virtual void updateImage(CLVideoDecoderOutput* frame, const QRectF& updateRect) override;

    static QSize getOptimalSize(const QSize& srcResolution, const QnItemDewarpingParams& itemDewarpingParams);
private:
    void updateFisheyeTransform(const QSize& imageSize, int plane);
    void updateFisheyeTransformRectilinear(const QSize& imageSize, int plane);
    void updateFisheyeTransformEquirectangular(const QSize& imageSize, int plane);
private:
    static const int MAX_COLOR_PLANES = 4;

    QnMediaDewarpingParams m_mediaDewarping;
    QnItemDewarpingParams m_itemDewarping;
    QSize m_lastImageSize;
    QPointF* m_transform[MAX_COLOR_PLANES];
    CLVideoDecoderOutput m_tmpBuffer;
    int m_lastImageFormat;
};

#endif // __FISHEYE_IMAGE_FILTER_H__
