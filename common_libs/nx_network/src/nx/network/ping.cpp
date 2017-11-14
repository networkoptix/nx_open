#include "ping.h"

#ifdef Q_OS_WIN
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	include <iphlpapi.h>
#else
#   include <stdlib.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <arpa/inet.h>
#   include <netinet/ip.h>
#   include <unistd.h>
#   ifndef Q_OS_IOS
#      include <netinet/ip_icmp.h>
#   endif
#endif

#include <QtCore/QProcess>

#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>


CLPing::CLPing()
{
}

bool CLPing::ping(const QString& ip, int retry, int /*timeoutPerRetry*/, int packetSize)
{
#if defined(Q_OS_WIN)
    QString cmd = QLatin1String("cmd /C ping %1 -n %2 -l %3");
    QProcess process;
    process.start(cmd.arg(ip).arg(retry).arg(packetSize));
    process.waitForFinished();
    return process.exitCode() == 0;
#elif defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    Q_UNUSED(ip)
    Q_UNUSED(retry)
    Q_UNUSED(packetSize)
    return false; // TODO: #android
#else
    QString cmd = QLatin1String("/bin/ping %1 -c %2 -s %3");
    int rez = system(cmd.arg(ip).arg(retry).arg(packetSize).toLatin1().data());
    return WEXITSTATUS(rez) == 0;
#endif
    // TODO: #ivan MacOS ping?
}
