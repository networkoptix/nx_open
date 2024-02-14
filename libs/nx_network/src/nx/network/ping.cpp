// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ping.h"

#include <nx/utils/system_network_headers.h>

#ifndef Q_OS_WIN
    #include <stdlib.h>
    #include <unistd.h>

    #include <netinet/ip.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #ifndef Q_OS_IOS
        #include <netinet/ip_icmp.h>
    #endif
#endif

#include <QtCore/QProcess>

#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>

CLPing::CLPing()
{
}

bool CLPing::ping(
    [[maybe_unused]] const QString& ip,
    [[maybe_unused]] int retry,
    int /*timeoutPerRetry*/,
    [[maybe_unused]] int packetSize)
{
#if defined(Q_OS_WIN)
    // Use the full path to ping.exe to prevent using the wrong one (e.g. `ping` from cygwin).
    QString cmd = QLatin1String("cmd /C \"%SystemRoot%\\System32\\ping.exe\" %1 -n %2 -l %3");
    QProcess process;
    process.start(cmd.arg(ip).arg(retry).arg(packetSize));
    process.waitForFinished();
    return process.exitCode() == 0;
#elif defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    return false;
#else
    QString cmd = QLatin1String("/bin/ping %1 -c %2 -s %3 > /dev/null 2>&1");
    int rez = system(cmd.arg(ip).arg(retry).arg(packetSize).toLatin1().data());
    return WEXITSTATUS(rez) == 0;
#endif
}
