#ifndef __QN_FILTER_HELPER_H__
#define __QN_FILTER_HELPER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include "core/resource/resource_media_layout.h"
#include "core/ptz/item_dewarping_params.h"
#include "utils/color_space/image_correction.h"
#include "abstract_image_filter.h"
#include "core/ptz/media_dewarping_params.h"

class QnImageFilterHelper
{
public:
    QnImageFilterHelper();

    void setVideoLayout(QnConstResourceVideoLayoutPtr layout); // tiled image filter
    void setCustomAR(qreal customAR); // tiled image filter
    void setRotation(int angle); // rotate image filter
    void setSrcRect(const QRectF& cropRect); // crop image filter
    void setDewarpingParams(const QnMediaDewarpingParams& params1, const QnItemDewarpingParams& params2); // fisheye image filter
    void setContrastParams(const ImageCorrectionParams& params); // contrast image filter
    void setTimeCorner(Qn::Corner corner, qint64 offset); // time image filter
    
    QList<QnAbstractImageFilterPtr> createFilterChain(const QSize& srcResolution) const;
    bool isEmpty() const;
private:
    QnConstResourceVideoLayoutPtr m_layout;
    qreal m_customAR;
    int m_rotAngle;
    QRectF m_cropRect;
    QnMediaDewarpingParams m_mediaDewarpingParams;
    QnItemDewarpingParams m_itemDewarpingParams;
    ImageCorrectionParams m_contrastParams;
    Qn::Corner m_timestampCorner;
    qint64 m_onscreenDateOffset;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __QN_FILTER_HELPER_H__
