#ifdef __APPLE__
#include "icmp.h"

#include <QtGlobal>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

bool Icmp::ping(const QString& hostAddr, int timeoutMSec) {
#ifdef Q_OS_MAC
    unsigned short id  = (unsigned short) (arc4random() >> 15);
#else
    unsigned short id  = (unsigned short) (random() >> 15);
#endif

    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (fd == -1)
    {
        // cl_log.
        printf("\tUnable to open handle.\n");
        return false;
    }

    struct
    {
        struct ip ip;
        struct icmp icmp;
    } packet;


    memset(&packet.icmp, 0, sizeof(packet.icmp));
    packet.icmp.icmp_type = ICMP_ECHO;
    packet.icmp.icmp_id = id;
#ifdef Q_OS_MAC
    packet.icmp.icmp_cksum = checksum(packet.icmp);
#endif

    in_addr_t addr = inet_addr(hostAddr.toLatin1().data());
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

    fd_set rset;

    FD_ZERO(&rset);
    FD_SET(fd, &rset);

    struct timeval tv;

    long usecs = timeoutMSec * 1000;
    tv.tv_usec = usecs % 1000000;
    tv.tv_sec = usecs / 1000000;

    int res = select(fd+1, &rset, NULL, NULL, &tv);

    if (res <= 0)
    {
        close(fd);
        return false;
    }

    // Have data to read
    if (FD_ISSET(fd, &rset))
    {
        res = recvfrom(fd, &packet, sizeof(packet), 0, (struct sockaddr *)&faddr, &len);

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
