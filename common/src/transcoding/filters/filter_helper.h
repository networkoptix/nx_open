#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <nx/core/transcoding/filters/filter_chain.h>

namespace nx { namespace core { namespace transcoding { struct LegacyTranscodingSettings; }}}

class QnImageFilterHelper
{
public:
    /**
     * Create filters for source image processing
     */
    static nx::core::transcoding::FilterChain createFilterChain(
        const nx::core::transcoding::LegacyTranscodingSettings& settings);
};

#endif // ENABLE_DATA_PROVIDERS
