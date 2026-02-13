// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <list>

#include <analytics/common/object_metadata.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>
#include <nx/core/transcoding/filters/transcoding_settings.h>
#include <nx/media/meta_data_packet.h>
#include <transcoding/filters/abstract_image_filter.h>

#include "abstract_metadata_filter.h"

class CLVideoDecoderOutput;
using CLVideoDecoderOutputPtr = QSharedPointer<CLVideoDecoderOutput>;

namespace nx::core::transcoding {

class NX_VMS_COMMON_API FilterChain
{
public:
    const static QSize kDefaultResolutionLimit;

    FilterChain(const Settings& settings);

    /**
     * Prepare set of filters to apply to video data.
     */
    void prepare(const QSize& srcFrameResolution,
        const QSize& resolutionLimit = kDefaultResolutionLimit);

    /**
     * Prepare set of filters to apply to an image. Main difference is that panoramic cameras
     * screenshots do not require tiling (already transcoded), but aspect ratio must be calculated
     * concerning video layout size.
     */
    void prepareForImage(const QSize& fullImageResolution);

    bool isImageTranscodingRequired(const QSize& fullImageResolution,
        const QSize& resolutionLimit = kDefaultResolutionLimit) const;

    bool isDownscaleRequired(const QSize& srcResolution) const;

    bool isReady() const;
    void reset();
    bool empty();

    void setMetadata(
        const std::vector<common::metadata::ObjectMetadataPacketPtr>& objectsData,
        const std::vector<QnConstMetaDataV1Ptr>& motionData);
    QSize apply(const QSize& resolution) const;
    CLVideoDecoderOutputPtr apply(const CLVideoDecoderOutputPtr& source) const;

    void addLegacyFilter(QnAbstractImageFilterPtr filter);

private:
    void prepareVideoArFilter(const QSize& srcFrameResolution);
    void prepareImageArFilter(const QSize& fullImageResolution);
    void prepareDewarpingFilter();
    void prepareZoomWindowFilter();
    void prepareImageEnhancementFilter();
    void prepareRotationFilter();
    void prepareOverlaysFilters();
    void prepareWatermarkFilter();
    void prepareDownscaleFilter(const QSize& srcFrameResolution, const QSize& resolutionLimit);
    void createScaleImageFilter(const QSize& dstSize);
    void createPixelationImageFilter();
    void createObjectInfoFilter();
    void createMotionFilter();

private:
    bool m_ready = false;
    Settings m_settings;
    std::list<QnAbstractImageFilterPtr> m_legacyFilters;
    std::list<QnAbstractImageFilterPtr> m_filters;
    std::list<AbstractMetadataFilterPtr> m_metadataFilters;
};

using FilterChainPtr = std::unique_ptr<FilterChain>;

} // namespace nx::core::transcoding
