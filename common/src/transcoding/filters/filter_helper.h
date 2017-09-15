#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <transcoding/filters/abstract_image_filter.h>

namespace nx { namespace core { namespace transcoding { struct LegacyTranscodingSettings; }}}

class QnImageFilterHelper
{
public:
    const static QSize kDefaultResolutionLimit;

    /**
     * Create filters for source image processing
     *
     * \param srcResolution             Source video size
     * \param srcResolution             Source video size
     * \returns                         Filter chain to process video
     */
    static QnAbstractImageFilterList createFilterChain(
        const nx::core::transcoding::LegacyTranscodingSettings& settings,
        const QSize& srcResolution,
        const QSize& resolutionLimit = kDefaultResolutionLimit);

    static QSize applyFilterChain(const QnAbstractImageFilterList& filters,
        const QSize& resolution);


};

#endif // ENABLE_DATA_PROVIDERS
