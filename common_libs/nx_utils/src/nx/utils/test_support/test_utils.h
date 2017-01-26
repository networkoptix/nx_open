#pragma once

/**@file
 * Convenience utilities for tests.
 *
 * Requirements before including this header:
 *     bool conf.forceLog;
 *     bool conf.enableHangOnFinish;
 *
 * Defined by this header:
 *     LOG(...);
 *     ASSERT(...);
 *     std::string qSizeToString(const QSize& size);
 *     void finishTest(bool hasFailure);
 */

#include <QtCore/QSize>

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>

#if !defined(OUTPUT_PREFIX)
    #define OUTPUT_PREFIX "[TEST] "
    #include <nx/utils/debug_utils.h>
#endif

#define LOG(ARG) do \
{ \
    if (conf.forceLog) \
        PRINT << (ARG); \
    else \
        NX_LOG((ARG), cl_logDEBUG1); \
} while (0)

#define ASSERT(ARG) NX_CRITICAL((ARG), "TEST INTERNAL ERROR")

using namespace std::placeholders;

static std::string qSizeToString(const QSize& size)
{
    return lit("%1 x %2").arg(size.width()).arg(size.height()).toLatin1().constData();
}

/**
 * Call at the end of each TEST_F():
 * <pre><code>
 *     finishTest(HasFailure());
 * </code></pre>
 */
static void finishTest(bool hasFailure)
{
    if (!conf.enableHangOnFinish)
        return;

    if (hasFailure)
        qWarning() << "\nFAILED; hanging...";
    else
        qWarning() << "\nPASSED; hanging...";

    for (;;) //< hang
    {
    }
}
