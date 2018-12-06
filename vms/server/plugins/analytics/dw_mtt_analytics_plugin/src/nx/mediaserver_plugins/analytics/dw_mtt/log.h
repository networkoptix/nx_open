#pragma once

#include <nx/kit/debug.h>

#if defined(NX_PRINT_TO_LOG)
    #include <QtCore/QStringLiteral>
    #include <nx/utils/log/log.h>

    #undef NX_PRINT
    #define NX_PRINT NX_UTILS_LOG_STREAM_NO_SPACE( \
        nx::utils::log::Level::debug, \
        nx::utils::log::Tag(QStringLiteral("dw_mtt_analytics_plugin")) \
    ) << NX_PRINT_PREFIX
#endif
