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
    m_codecID(AV_CODEC_ID_NONE)
{
}

void QnImageFilterHelper::setVideoLayout(const QnConstResourceVideoLayoutPtr &layout)
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

void QnImageFilterHelper::setTimeStampParams(const QnTimeStampParams& params)
{
    m_timestampParams = params;
}

void QnImageFilterHelper::setCodec( AVCodecID codecID )
{
    m_codecID = codecID;
}

bool QnImageFilterHelper::isEmpty() const
{
    if (!qFuzzyIsNull(m_customAR))
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
    if (m_timestampParams.enabled)
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

const QSize QnImageFilterHelper::defaultResolutionLimit(8192 - 16, 8192 - 16);

QList<QnAbstractImageFilterPtr> QnImageFilterHelper::createFilterChain(const QSize& srcResolution, const QSize& resolutionLimit) const
{
    static const float MIN_STEP_CHANGE_COEFF = 0.95f;
    static const float ASPECT_RATIO_COMPARISION_PRECISION = 0.01f;

    QList<QnAbstractImageFilterPtr> result;

    if (!qFuzzyIsNull(m_customAR)) {
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

    if (m_timestampParams.enabled)
        result << QnAbstractImageFilterPtr(new QnTimeImageFilter(m_layout, m_timestampParams));

    //verifying that output resolution is supported by codec

    //adjusting output resolution if needed
    for( qreal prevResizeRatio = 1.0; prevResizeRatio > 0.07; )
    {
        //we can't be sure the way input image scale affects output, so adding a loop...

        const QSize resultResolution = updatedResolution( result, srcResolution );
        if( resultResolution.width() <= resolutionLimit.width() &&
            resultResolution.height() <= resolutionLimit.height() )
        {
            break;  //resolution is OK
        }

        qreal resizeRatio = 1.0;

        NX_ASSERT( resultResolution.width() > 0 && resultResolution.height() > 0 );

        if( resultResolution.width() > resolutionLimit.width() &&
            resultResolution.width() > 0 )
        {
            resizeRatio = (qreal)resolutionLimit.width() / resultResolution.width();
        }
        if( resultResolution.height() > resolutionLimit.height() &&
            resultResolution.height() > 0 )
        {
            resizeRatio = std::min<qreal>(
                resizeRatio,
                (qreal)resolutionLimit.height() / resultResolution.height() );
        }

        if( resizeRatio >= 1.0 )
            break;  //done. resolution is OK

        if( resizeRatio >= prevResizeRatio - ASPECT_RATIO_COMPARISION_PRECISION )
            resizeRatio = prevResizeRatio * MIN_STEP_CHANGE_COEFF;   //this is needed for the loop to be finite
        prevResizeRatio = resizeRatio;

        //adjusting scale filter
        const QSize resizeToSize = QnCodecTranscoder::roundSize(
            QSize( srcResolution.width() * resizeRatio, srcResolution.height() * resizeRatio ) );

        auto scaleFilter = result.front().dynamicCast<QnScaleImageFilter>();
        if( !scaleFilter )
        {
            //adding scale filter
            scaleFilter.reset( new QnScaleImageFilter( resizeToSize ) );
            result.push_front( scaleFilter );
        }
        else
        {
            scaleFilter->setOutputImageSize( resizeToSize );
        }
    }

    return result;
}

#endif // ENABLE_DATA_PROVIDERS
