#ifdef __APPLE__
#include "icmp_mac.h"

#include <QtGlobal>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>


/*
 * in_cksum --
 *	Checksum routine for Internet Protocol family headers (C Version)
 */
u_short in_cksum(u_short *addr, int len)
{
    int nleft, sum;
    u_short *w;
    union {
        u_short	us;
        u_char	uc[2];
    } last;
    u_short answer;

    nleft = len;
    sum = 0;
    w = addr;

    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while (nleft > 1)  {
        sum += *w++;
        nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1) {
        last.uc[0] = *(u_char *)w;
        last.uc[1] = 0;
        sum += last.us;
    }

    /* add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
    sum += (sum >> 16);			/* add carry */
    answer = ~sum;				/* truncate to 16 bits */
    return(answer);
}

bool Icmp::ping(const QString& hostAddr, int timeoutMSec) {
    unsigned short id  = (unsigned short) (arc4random() >> 15);
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (fd == -1) {
        qDebug() << "Unable to open handle.";
        return false;
    }

    struct {
        struct ip ip;
        struct icmp icmp;
    } packet;


    memset(&packet.icmp, 0, sizeof(packet.icmp));
    packet.icmp.icmp_type = ICMP_ECHO;
    packet.icmp.icmp_id = id;
    packet.icmp.icmp_cksum = in_cksum((unsigned short *) &packet.icmp, sizeof(packet.icmp));

    in_addr_t addr = inet_addr(hostAddr.toLatin1().data());
    if (addr == INADDR_NONE)
    {
        qDebug() << "Failed conversion" << hostAddr << "to inet addr";
        return false;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));

    saddr.sin_family = AF_INET;
    saddr.sin_port = 0;
    saddr.sin_addr.s_addr = addr;
    saddr.sin_len = sizeof(struct sockaddr_in);

    if (sendto(fd, (void*)&packet.icmp, sizeof (packet.icmp), 0, (sockaddr*)&saddr, sizeof(saddr)) == -1) {
        close(fd);
        qDebug() << "Error when sending packet";
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
        if (res == 0)
            qDebug() << "Ping timeout";
        else
            qDebug() << "Waiting was interrupted" << res;
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
            qDebug() << "Ping reply received successfully";
            return true;
        }
    }

    qDebug() << "Ping timeout";
    close(fd);
    return false;
}

#endif
