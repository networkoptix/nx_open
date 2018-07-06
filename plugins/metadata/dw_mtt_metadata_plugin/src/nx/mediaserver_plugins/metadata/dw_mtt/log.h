#pragma once

#if defined(NX_PRINT_TO_LOG)
    #include <nx/utils/literal.h>
    #include <nx/utils/log/log.h>
    #define NX_PRINT NX_UTILS_LOG_STREAM_NO_SPACE( \
        nx::utils::log::Level::debug, nx::utils::log::Tag(lit("dw_mtt_metadata_plugin"))) \
        << NX_PRINT_PREFIX
#endif

#include <nx/kit/debug.h>
