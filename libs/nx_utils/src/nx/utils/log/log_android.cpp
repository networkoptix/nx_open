// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log_writers.h"

#include <android/log.h>

#include <QtCore/QCoreApplication>

namespace nx::log {

static android_LogPriority toAndroidLogLevel(Level logLevel)
{
    switch (logLevel)
    {
        case Level::undefined:
            return ANDROID_LOG_UNKNOWN;

        case Level::none:
            return ANDROID_LOG_SILENT;

        case Level::error:
            return ANDROID_LOG_ERROR;

        case Level::warning:
            return ANDROID_LOG_WARN;

        case Level::info:
            return ANDROID_LOG_INFO;

        case Level::debug:
            return ANDROID_LOG_DEBUG;

        case Level::verbose:
        case Level::trace: //< Android does not have an equivalent log level
            return ANDROID_LOG_VERBOSE;

        default:
            return ANDROID_LOG_DEFAULT;
    }
}

void StdOut::writeImpl(Level level, const QString& message)
{
    __android_log_print(
        toAndroidLogLevel(level),
        QCoreApplication::applicationName().toUtf8().constData(),
        "%s",
        message.toUtf8().constData());
}

} // namespace nx::log
