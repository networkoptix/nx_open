// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>
#include <nx/vms/api/data/pixelation_settings.h>

#include <transcoding/filters/abstract_image_filter.h>

namespace nx::core::transcoding {

class NX_VMS_COMMON_API PixelationImageFilter: public QnAbstractImageFilter
{
public:
    PixelationImageFilter(const nx::vms::api::PixelationSettings& settings);
    ~PixelationImageFilter();

    static void pixelateImage(QImage* image, QList<QRect> m_blurRectangles, double intensity);

    virtual CLVideoDecoderOutputPtr updateImage(
        const CLVideoDecoderOutputPtr& frame) override;

    virtual QSize updatedResolution(const QSize& sourceSize) { return sourceSize; };

    void setMetadata(const std::list<QnConstAbstractCompressedMetadataPtr>& metadata);

private:
    nx::vms::api::PixelationSettings m_settings;
    std::list<QnConstAbstractCompressedMetadataPtr> m_metadata;
};

} // nx::core::transcoding
