// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>
#include <nx/core/transcoding/filters/transcoding_settings.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <transcoding/filters/abstract_image_filter.h>

class CLVideoDecoderOutput;
using CLVideoDecoderOutputPtr = QSharedPointer<CLVideoDecoderOutput>;

namespace nx {
namespace core {
namespace transcoding {

class NX_VMS_COMMON_API FilterChain: public QnAbstractImageFilterList
{
public:
    const static QSize kDefaultResolutionLimit;

    FilterChain() = default;

    explicit FilterChain(const Settings& settings,
        nx::vms::api::dewarping::MediaData dewarpingParams, // TODO move it to settings?
        QnConstResourceVideoLayoutPtr layout);

    FilterChain(const FilterChain&) = default;
    FilterChain& operator=(const FilterChain&) = default;

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
    void prepareForImage(const QSize& fullImageResolution,
        const QSize& resolutionLimit = kDefaultResolutionLimit);

    /**
     * Check if chain contains any options that require transcoding.
     * @param concernTiling If video layout transcoding (tiling) matters - it is not applied while
     *     transcoding images.
     */
    bool isTranscodingRequired(bool concernTiling = true) const;

    bool isImageTranscodingRequired(const QSize& fullImageResolution,
        const QSize& resolutionLimit = kDefaultResolutionLimit) const;

    bool isDownscaleRequired(const QSize& srcResolution) const;

    bool isReady() const;
    void reset();

    QSize apply(const QSize& resolution) const;
    CLVideoDecoderOutputPtr apply(
        const CLVideoDecoderOutputPtr& source,
        const QnAbstractCompressedMetadataPtr& metadata) const;

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

private:
    bool m_ready = false;
    Settings m_settings;
    nx::vms::api::dewarping::MediaData m_dewarpingParams;
    QnConstResourceVideoLayoutPtr m_layout;
    QnAbstractImageFilterList m_legacyFilters;
};

} // namespace transcoding
} // namespace core
} // namespace nx
