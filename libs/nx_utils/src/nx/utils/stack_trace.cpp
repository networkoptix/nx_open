// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stack_trace.h"

#include <sstream>

#if defined(__APPLE__) && !defined(_GNU_SOURCE)
    #define _GNU_SOURCE
#endif

#include <boost/stacktrace.hpp>

#include <QtCore/QProcess>

#include <nx/build_info.h>
#include <nx/utils/log/format.h>

namespace nx::utils {

std::string stackTrace()
{
    std::stringstream ss;

    #if defined(_DEBUG)
        static constexpr bool isDebugBuild = true;
    #else
        static constexpr bool isDebugBuild = false;
    #endif

    if (nx::build_info::isMacOsX() && isDebugBuild)
    {
        QStringList args = {"-p", QString::number(getpid()), "--fullPath"};

        const auto stackData = boost::stacktrace::stacktrace();

        QStringList hexAddresses;

        for (const auto& f: stackData)
            hexAddresses << QString("0x%1").arg((qint64) f.address(), 0, 16);

        args += hexAddresses;

        #if !defined(Q_OS_IOS)
            QProcess p;
            p.start("atos", args);
            if (p.waitForFinished())
            {
                QString output(p.readAllStandardOutput());

                QStringList lines = output.split('\n');
                if (!lines.isEmpty() && lines.last().isEmpty())
                    lines.removeLast();

                const auto frameCount = std::min<qsizetype>(lines.size(), stackData.size());

                for (qsizetype i = 0; i < frameCount; ++i)
                    ss << nx::format("#%1 %2 %3\n", i, hexAddresses[i], lines[i]).toStdString();
            }
            else
            {
                ss << stackData;
            }
        #else
            ss << stackData;
        #endif
    }
    else
    {
        ss << boost::stacktrace::stacktrace();
    }

    return ss.str();
}

} // namespace nx::utils
