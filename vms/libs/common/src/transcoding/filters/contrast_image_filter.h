#ifndef __GAMMA_IMAGE_FILTER_H__
#define __GAMMA_IMAGE_FILTER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include <utils/color_space/image_correction.h>

#include "abstract_image_filter.h"


class QnContrastImageFilter: public QnAbstractImageFilter
{
public:
    QnContrastImageFilter(const nx::vms::api::ImageCorrectionData& params);
    CLVideoDecoderOutputPtr updateImage(const CLVideoDecoderOutputPtr& frame) override;
    virtual QSize updatedResolution(const QSize& srcSize) override { return srcSize; }
private:
    bool isFormatSupported(CLVideoDecoderOutput* frame) const;

private:
    nx::vms::api::ImageCorrectionData m_params;
    ImageCorrectionResult m_gamma;
    float m_lastGamma;
    quint8 m_gammaCorrection[256];
    CLVideoDecoderOutputPtr m_deepCopyFrame;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __GAMMA_IMAGE_FILTER_H__
