#pragma once

#if defined(NX_PRINT_TO_LOG)
    #include <nx/utils/log/log.h>
    #include <nx/utils/literal.h>
    #define NX_PRINT NX_UTILS_LOG_STREAM_NO_SPACE( \
        nx::utils::log::Level::debug, nx::utils::log::Tag(lit("ssc_metadata_plugin"))) \
            << NX_PRINT_PREFIX
#endif

#include <nx/kit/debug.h>
