#include "filter_helper.h"
#include "scale_image_filter.h"
#include "tiled_image_filter.h"
#include "crop_image_filter.h"
#include "fisheye_image_filter.h"
#include "contrast_image_filter.h"
#include "rotate_image_filter.h"
#include "time_image_filter.h"
#include "../transcoder.h"

#ifdef ENABLE_DATA_PROVIDERS

QnImageFilterHelper::QnImageFilterHelper():
    m_customAR(0.0),
    m_rotAngle(0),
    m_timestampCorner(Qn::NoCorner),
    m_onscreenDateOffset(0),
    m_timeMs(0)
{
}

void QnImageFilterHelper::setVideoLayout(QnConstResourceVideoLayoutPtr layout)
{
    m_layout = layout;
}

void QnImageFilterHelper::setCustomAR(qreal customAR)
{
    m_customAR = customAR;
}

void QnImageFilterHelper::setRotation(int angle)
{
    m_rotAngle = angle;
}

void QnImageFilterHelper::setSrcRect(const QRectF& cropRect)
{
    m_cropRect = cropRect;
}

void QnImageFilterHelper::setDewarpingParams(const QnMediaDewarpingParams& params1, const QnItemDewarpingParams& params2)
{
    m_mediaDewarpingParams = params1;
    m_itemDewarpingParams = params2;
}

void QnImageFilterHelper::setContrastParams(const ImageCorrectionParams& params)
{
    m_contrastParams = params;
}

void QnImageFilterHelper::setTimeCorner(Qn::Corner corner, qint64 onscreenDateOffset, qint64 timeMsec)
{
    m_timestampCorner = corner;
    m_onscreenDateOffset = onscreenDateOffset;
    m_timeMs = timeMsec;
}

bool QnImageFilterHelper::isEmpty() const
{
    if (m_customAR)
        return false;
    if (m_layout && m_layout->channelCount() > 1)
        return false;
    if (!m_cropRect.isEmpty())
        return false;
    if (m_itemDewarpingParams.enabled)
        return false;
    if (m_contrastParams.enabled)
        return false;
    if (m_rotAngle)
        return false;
    if (m_timestampCorner != Qn::NoCorner) 
        return false;

    return true;
}

QSize QnImageFilterHelper::updatedResolution(const QList<QnAbstractImageFilterPtr>& filters, const QSize& srcResolution) const
{
    QSize result(srcResolution);
    for (auto filter: filters)
        result = filter->updatedResolution(result);
    return result;
}

QList<QnAbstractImageFilterPtr> QnImageFilterHelper::createFilterChain(const QSize& srcResolution) const
{
    QList<QnAbstractImageFilterPtr> result;

    if (m_customAR != 0.0) {
        QSize newSize;
        newSize.setHeight(srcResolution.height());
        newSize.setWidth(srcResolution.height() * m_customAR + 0.5);
        if (newSize != srcResolution)
            result << QnAbstractImageFilterPtr(new QnScaleImageFilter(QnCodecTranscoder::roundSize(newSize)));
    }

    bool tiledFilterExist = false;
    if (m_layout && m_layout->channelCount() > 1) {
        result << QnAbstractImageFilterPtr(new QnTiledImageFilter(m_layout));
        tiledFilterExist = true;
    }

    if (!m_cropRect.isEmpty() && !m_itemDewarpingParams.enabled) {
        result << QnAbstractImageFilterPtr(new QnCropImageFilter(m_cropRect));
    }
    if (m_itemDewarpingParams.enabled)
        result << QnAbstractImageFilterPtr(new QnFisheyeImageFilter(m_mediaDewarpingParams, m_itemDewarpingParams));
    if (m_contrastParams.enabled)
        result << QnAbstractImageFilterPtr(new QnContrastImageFilter(m_contrastParams, tiledFilterExist));
    if (m_rotAngle)
        result << QnAbstractImageFilterPtr(new QnRotateImageFilter(m_rotAngle));

    if (m_timestampCorner != Qn::NoCorner) 
        result << QnAbstractImageFilterPtr(new QnTimeImageFilter(m_layout, m_timestampCorner, m_onscreenDateOffset, m_timeMs));

    return result;
}

#endif // ENABLE_DATA_PROVIDERS
