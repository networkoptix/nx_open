// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/core/transcoding/filters/filter_chain.h>

namespace nx { namespace core { namespace transcoding { struct LegacyTranscodingSettings; }}}

class NX_VMS_COMMON_API QnImageFilterHelper
{
public:
    /**
     * Create filters for source image processing
     */
    static nx::core::transcoding::FilterChain createFilterChain(
        const nx::core::transcoding::LegacyTranscodingSettings& settings);
};
