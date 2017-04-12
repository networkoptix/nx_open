#include "log.h"

#include <android/log.h>

#include <QtCore/QCoreApplication>

namespace {

android_LogPriority toAndroidLogLevel(QnLogLevel logLevel)
{
    switch (logLevel)
    {
        case cl_logUNKNOWN:
            return ANDROID_LOG_UNKNOWN;
        case cl_logNONE:
            return ANDROID_LOG_SILENT;
        case cl_logALWAYS:
            return ANDROID_LOG_INFO;
        case cl_logERROR:
            return ANDROID_LOG_ERROR;
        case cl_logWARNING:
            return ANDROID_LOG_WARN;
        case cl_logINFO:
            return ANDROID_LOG_INFO;
        case cl_logDEBUG1:
            return ANDROID_LOG_DEBUG;
        case cl_logDEBUG2:
            return ANDROID_LOG_VERBOSE;
        default:
            return ANDROID_LOG_DEFAULT;
    }
}

} // namespace

void QnLog::writeToStdout(const QString& str, QnLogLevel logLevel)
{
    __android_log_print(
        toAndroidLogLevel(logLevel),
        QCoreApplication::applicationName().toUtf8().constData(),
        "%s",
        str.toUtf8().constData());
}
