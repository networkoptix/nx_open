#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <nx/core/transcoding/filters/filter_chain.h>

namespace nx { namespace core { namespace transcoding { struct LegacyTranscodingSettings; }}}

class QnImageFilterHelper
{
public:
    /**
     * Create filters for source image processing
     *
     * \param srcResolution             Source video size
     * \param srcResolution             Source video size
     * \returns                         Filter chain to process video
     */
    static nx::core::transcoding::FilterChain createFilterChain(
        const nx::core::transcoding::LegacyTranscodingSettings& settings,
        const QSize& srcResolution,
        const QSize& resolutionLimit = nx::core::transcoding::FilterChain::kDefaultResolutionLimit);
};

#endif // ENABLE_DATA_PROVIDERS
