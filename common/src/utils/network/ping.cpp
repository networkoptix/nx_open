#include "ping.h"

#ifdef Q_OS_WIN
#   include <icmpapi.h>
#   include <stdio.h>
#else
#   include <stdlib.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <arpa/inet.h>
#   include <netinet/ip.h>
#   include <netinet/ip_icmp.h>
#   include <unistd.h>
#endif

#include <QtCore/QProcess>

#include <utils/common/log.h>
#include <utils/common/systemerror.h>


CLPing::CLPing()
{
}

bool CLPing::ping(const QString& ip, int retry, int /*timeoutPerRetry*/, int packetSize)
{
#ifdef Q_OS_WIN
    QString cmd = QLatin1String("cmd /C ping %1 -n %2 -l %3");
    QProcess process;
    process.start(cmd.arg(ip).arg(retry).arg(packetSize));
    process.waitForFinished();
    return process.exitCode() == 0;
#else
    QString cmd = QLatin1String("/bin/ping %1 -c %2 -s %3");
    int rez = system(cmd.arg(ip).arg(retry).arg(packetSize).toLatin1().data());
    return WEXITSTATUS(rez) == 0;
#endif 
    //TODO: #ivan MacOS ping?
}
