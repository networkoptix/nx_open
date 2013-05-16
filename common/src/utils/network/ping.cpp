#include "ping.h"

#ifdef Q_OS_WIN
#   include <icmpapi.h>
#   include <stdio.h>
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <arpa/inet.h>
#   include <netinet/ip.h>
#   include <netinet/ip_icmp.h>
#   include <unistd.h>
#endif

#include <utils/common/log.h>
#include <utils/common/systemerror.h>


CLPing::CLPing()
{
}

#ifdef Q_OS_WIN
bool CLPing::ping(const QString& ip, int retry, int timeoutPerRetry, int packetSize)
{
    Q_UNUSED(packetSize)
    // Declare and initialize variables

    HANDLE hIcmpFile;
    unsigned long ipaddr = INADDR_NONE;
    DWORD dwRetVal = 0;
    DWORD dwError = 0;
    char SendData[] = "Data Buffer";

    const DWORD ReplySize = sizeof (ICMP_ECHO_REPLY) + sizeof (SendData) + 8;;
    LPVOID ReplyBuffer[ReplySize];

    ipaddr = inet_addr(ip.toLatin1().data());
    if (ipaddr == INADDR_NONE)
        return false;

    hIcmpFile = IcmpCreateFile();
    if (hIcmpFile == INVALID_HANDLE_VALUE)
    {
        cl_log.log(QLatin1String("CLPing: Unable to open handle "), cl_logERROR);
        //printf("\tUnable to open handle.\n");
        //printf("IcmpCreatefile returned error: %ld\n", GetLastError());
        return false;
    }

    // Allocate space for at a single reply
    dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, SendData, sizeof (SendData), NULL,
        ReplyBuffer, ReplySize, retry * timeoutPerRetry);
    if (dwRetVal != 0)
    {
        PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY) ReplyBuffer;
        struct in_addr ReplyAddr;
        ReplyAddr.S_un.S_addr = pEchoReply->Address;

        /*/
        if (dwRetVal > 1)
        {
            printf("\tReceived %ld icmp message responses\n", dwRetVal);
            printf("\tInformation from the first response:\n");
        }
        else
        {
            printf("\tReceived %ld icmp message response\n", dwRetVal);
            printf("\tInformation from this response:\n");
        }

        printf("\t  Received from %s\n", inet_ntoa(ReplyAddr));
        printf("\t  Status = %ld  ", pEchoReply->Status);
        */

        switch (pEchoReply->Status)
        {
        case IP_DEST_HOST_UNREACHABLE:
            //printf("(Destination host was unreachable)\n");
            return false;
            break;
        case IP_DEST_NET_UNREACHABLE:
            //printf("(Destination Network was unreachable)\n");
            return false;
            break;
        case IP_REQ_TIMED_OUT:
            //printf("(Request timed out)\n");
            return false;
            break;
        default:
            //printf("\n");
            return true;
            break;
        }

        //printf("\t  Roundtrip time = %ld milliseconds\n",pEchoReply->RoundTripTime);
    }
    else
    {
        qWarning() << "Failed to ping camera" << ip << "error:" << SystemError::getLastOSErrorText();
        return false;
    }
}

#else

unsigned short checksum(const struct icmp& packet)
{
    uint i, j;

    for (j = 0, i = 0; i < sizeof(struct icmp) / 2; i++)
      j += ((quint16 *)&packet)[i];
    while (j>>16)
      j = (j & 0xffff) + (j >> 16);

    return (j == 0xffff) ? j : ~j;
}

bool CLPing::ping(const QString& ip, int retry, int timeoutPerRetry, int packetSize)
{
    Q_UNUSED(packetSize)

    long rnd;

#ifdef Q_OS_MAC
    rnd = arc4random();
#else
    rnd = random();
#endif

    unsigned short id  = (unsigned short) (rnd >> 15);

#ifdef Q_OS_MAC
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
#else
    int fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
#endif
    if (fd == -1)
    {        
        cl_log.log(cl_logWARNING, "CLPing::ping(): Unable to open raw socket: %s", strerror(errno));
        return false;
    } else
    {
        //cl_log.log(cl_logWARNING, "CLPing::ping(): SUCCESS");
    }

    struct
    {
        struct ip ip;
        struct icmp icmp;
    } packet;


    memset(&packet.icmp, 0, sizeof(packet.icmp));
    packet.icmp.icmp_type = ICMP_ECHO;
    packet.icmp.icmp_id = id;
    packet.icmp.icmp_cksum = checksum(packet.icmp);

    in_addr_t addr = inet_addr(ip.toLatin1().data());
    if (addr == INADDR_NONE)
    {
        return false;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));

    saddr.sin_family = AF_INET;
    saddr.sin_port = 0;
    saddr.sin_addr.s_addr = addr;

#ifdef Q_OS_MAC
    saddr.sin_len = sizeof(struct sockaddr_in);
#endif

    if (sendto(fd, (void*)&packet.icmp, sizeof (packet.icmp), 0, (sockaddr*)&saddr, sizeof(saddr)) == -1)
    {
        close(fd);
        // cl_log.
        return false;
    }

    struct sockaddr_in faddr;
    socklen_t len = sizeof(faddr);

    memset(&packet, 0, sizeof(packet));

    struct timeval tv;
    long usecs = timeoutPerRetry * 1000;
    tv.tv_usec = usecs % 1000000;
    tv.tv_sec = usecs / 1000000;
    if( ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const void*)&tv, sizeof(tv)) < 0 )
    {
        close(fd);
        return false;
    }

    for( int i = 0; i < retry; i++ )
    {
        int res = recvfrom( fd, &packet, sizeof(packet), 0, (struct sockaddr *)&faddr, &len );

        if( res < 0 )
        {
            if( errno == EAGAIN || errno == EWOULDBLOCK )
                continue;   //timed out
            close(fd);
            return false;
        }

        if (res == sizeof(packet) &&
                saddr.sin_addr.s_addr == faddr.sin_addr.s_addr &&
                packet.icmp.icmp_type == ICMP_ECHOREPLY &&
                packet.icmp.icmp_seq == 0 &&
                packet.icmp.icmp_id == id)
        {
            close(fd);
            return true;
        }
    }

    close(fd);
    return false;
}

#endif
