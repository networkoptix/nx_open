// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/image_correction_data.h>
#include <utils/color_space/image_correction.h>

#include "abstract_image_filter.h"

class QnContrastImageFilter: public QnAbstractImageFilter
{
public:
    QnContrastImageFilter(const nx::vms::api::ImageCorrectionData& params);

    virtual CLVideoDecoderOutputPtr updateImage(
        const CLVideoDecoderOutputPtr& frame,
        const QnAbstractCompressedMetadataPtr& metadata) override;

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
