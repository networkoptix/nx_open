#ifndef __FISHEYE_IMAGE_FILTER_H__
#define __FISHEYE_IMAGE_FILTER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QScopedPointer>

#include <core/ptz/media_dewarping_params.h>
#include <core/ptz/item_dewarping_params.h>

#include "abstract_image_filter.h"

class QnFisheyeImageFilter: public QnAbstractImageFilter
{
public:
    QnFisheyeImageFilter(const QnMediaDewarpingParams& mediaDewarping, const QnItemDewarpingParams& itemDewarping);

    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame, const QRectF& updateRect, qreal ar) override;

    static QSize getOptimalSize(const QSize& srcResolution, const QnItemDewarpingParams& itemDewarpingParams);
    virtual QSize updatedResolution(const QSize& srcSize) override;
private:
    void updateFisheyeTransform(const QSize& imageSize, int plane, qreal ar);
    void updateFisheyeTransformRectilinear(const QSize& imageSize, int plane, qreal ar);
    void updateFisheyeTransformEquirectangular(const QSize& imageSize, int plane, qreal ar);

private:
    static const int MAX_COLOR_PLANES = 4;

    QnMediaDewarpingParams m_mediaDewarping;
    QnItemDewarpingParams m_itemDewarping;
    QSize m_lastImageSize;
    QPointF* m_transform[MAX_COLOR_PLANES];
    QScopedPointer<CLVideoDecoderOutput> m_tmpBuffer;
    int m_lastImageFormat;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __FISHEYE_IMAGE_FILTER_H__
