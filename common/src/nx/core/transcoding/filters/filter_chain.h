#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>

#include <nx/core/transcoding/filters/transcoding_settings.h>

#include <transcoding/filters/abstract_image_filter.h>

struct QnMediaDewarpingParams;
class CLVideoDecoderOutput;
using CLVideoDecoderOutputPtr = QSharedPointer<CLVideoDecoderOutput>;

namespace nx {
namespace core {
namespace transcoding {

class FilterChain: public QnAbstractImageFilterList
{
public:
    const static QSize kDefaultResolutionLimit;

    FilterChain() = default;
    explicit FilterChain(const Settings& settings);
    FilterChain(const FilterChain&) = default;
    FilterChain& operator=(const FilterChain&) = default;

    /**
     * Prepare set of filters to apply to video data.
     */
    void prepare(const QnMediaResourcePtr& resource,
        const QSize& srcFrameResolution,
        const QSize& resolutionLimit = kDefaultResolutionLimit);

    /**
     * Prepare set of filters to apply to an image. Main difference is that panoramic cameras
     * screenshots do not require tiling (already transcoded), but aspect ratio must be calculated
     * concerning video layout size.
     */
    void prepareForImage(const QnMediaResourcePtr& resource,
        const QSize& fullImageResolution,
        const QSize& resolutionLimit = kDefaultResolutionLimit);

    bool isImageTranscodingRequired(const QSize& fullImageResolution,
        const QSize& resolutionLimit = kDefaultResolutionLimit) const;
    bool isTranscodingRequired(const QnConstResourceVideoLayoutPtr& videoLayout) const;
    bool isTranscodingRequired(const QnMediaResourcePtr& resource) const;

    bool isDownscaleRequired(const QSize& srcResolution) const;

    bool isReady() const;
    void reset();

    QSize apply(const QSize& resolution) const;
    CLVideoDecoderOutputPtr apply(const CLVideoDecoderOutputPtr& source) const;

    void addLegacyFilter(QnAbstractImageFilterPtr filter);

private:
    void prepareVideoArFilter(const QSize& srcFrameResolution);
    void prepareImageArFilter(const QnMediaResourcePtr& resource,
        const QSize& fullImageResolution);
    void prepareDewarpingFilter(const QnMediaResourcePtr& resource);
    void prepareZoomWindowFilter();
    void prepareImageEnhancementFilter();
    void prepareRotationFilter();
    void prepareOverlaysFilters();
    void prepareDownscaleFilter(const QSize& srcFrameResolution, const QSize& resolutionLimit);

private:
    bool m_ready = false;
    Settings m_settings;
    QnAbstractImageFilterList m_legacyFilters;
};

} // namespace transcoding
} // namespace core
} // namespace nx

#endif // ENABLE_DATA_PROVIDERS
