#ifndef __GAMMA_IMAGE_FILTER_H__
#define __GAMMA_IMAGE_FILTER_H__

#include "utils/color_space/image_correction.h"
#include "abstract_filter.h"


class QnContrastImageFilter: public QnAbstractImageFilter
{
public:
    QnContrastImageFilter(const ImageCorrectionParams& params);
    virtual void updateImage(CLVideoDecoderOutput* frame, const QRectF& updateRect) override;
private:
    ImageCorrectionParams m_params;
    ImageCorrectionResult m_gamma;
    float m_lastGamma;
    quint8 m_gammaCorrection[256];
};

#endif // __GAMMA_IMAGE_FILTER_H__
