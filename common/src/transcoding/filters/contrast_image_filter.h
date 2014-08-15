#ifndef __GAMMA_IMAGE_FILTER_H__
#define __GAMMA_IMAGE_FILTER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include "utils/color_space/image_correction.h"

#include "abstract_image_filter.h"


class QnContrastImageFilter: public QnAbstractImageFilter
{
public:
    QnContrastImageFilter(const ImageCorrectionParams& params);
    virtual void updateImage(CLVideoDecoderOutput* frame, const QRectF& updateRect, qreal ar) override;

private:
    bool isFormatSupported(CLVideoDecoderOutput* frame) const;

private:
    ImageCorrectionParams m_params;
    ImageCorrectionResult m_gamma;
    float m_lastGamma;
    quint8 m_gammaCorrection[256];
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __GAMMA_IMAGE_FILTER_H__
