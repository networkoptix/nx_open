#pragma once
#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QScopedPointer>
#include <QtCore/QVector>

#include <core/ptz/media_dewarping_params.h>
#include <core/ptz/item_dewarping_params.h>
#include <utils/media/frame_info.h>

#include "abstract_image_filter.h"

class QnFisheyeImageFilter: public QnAbstractImageFilter
{
public:
    QnFisheyeImageFilter(const QnMediaDewarpingParams& mediaDewarping, const QnItemDewarpingParams& itemDewarping);

    virtual CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;
    virtual QSize updatedResolution(const QSize& srcSize) override;
private:
    void updateFisheyeTransform(const QSize& imageSize, int plane, qreal ar);
    void updateFisheyeTransformRectilinear(const QSize& imageSize, int plane, qreal ar);
    void updateFisheyeTransformEquirectangular(const QSize& imageSize, int plane, qreal ar);
    static QSize getOptimalSize(const QSize& srcResolution, const QnItemDewarpingParams& itemDewarpingParams);
private:
    static const int MAX_COLOR_PLANES = 4;

    QnMediaDewarpingParams m_mediaDewarping;
    QnItemDewarpingParams m_itemDewarping;
    QSize m_lastImageSize;

    typedef QVector<QPointF> PointsVector;
    PointsVector m_transform[MAX_COLOR_PLANES];
    QScopedPointer<CLVideoDecoderOutput> m_tmpBuffer;
    int m_lastImageFormat;
};

#endif // ENABLE_DATA_PROVIDERS
