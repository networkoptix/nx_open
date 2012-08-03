#ifndef __SIMPLE_TFTP_CLIENT__1117
#define __SIMPLE_TFTP_CLIENT__1117
#include <QString>
#include "utils/network/socket.h"

class QnByteArray;

// this class is designed just to be friendly with AV cams;( I doubd it can be usefull with smth else)
class CLSimpleTFTPClient
{

    enum {all_ok,  time_out};
    enum {blk_size=1450, double_blk_size=2904};

    // arecont vision cams supports 2 blk sizes.
    // I think second one is better 
    //1) you save cpu time, coz you call sendto function 2 times less ( huge savings on windows )
    //2) more friendly with wireless network ( it hates acks).

public:
    CLSimpleTFTPClient(const QHostAddress& ip, unsigned int timeout, unsigned int retry);
    ~CLSimpleTFTPClient(){};

    const QHostAddress& getHostAddress() const { return m_hostAddress; }

    int read(const QString& fn, QnByteArray& data);
    const unsigned char* getLastPacket(int& size) const
    {
        size = m_last_packet_size;
        return m_last_packet;
    }

private:
    int form_read_request(const QString& fn, char* buff);
    int form_ack(unsigned short blk, char* buff);

private:

    unsigned char m_last_packet[3100];
    unsigned int m_last_packet_size;

    int m_retry;
    QHostAddress m_hostAddress;
    QString m_ip;
    int m_timeout;

    int m_status;
    int m_wish_blk_size;
    int m_curr_blk_size;

    UDPSocket m_sock;
};

#endif //__SIMPLE_TFTP_CLIENT__1117
