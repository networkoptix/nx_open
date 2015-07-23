#ifndef __SIMPLE_TFTP_CLIENT__1117
#define __SIMPLE_TFTP_CLIENT__1117

#ifdef ENABLE_ARECONT

#include <memory>

#include <QString>

#include "utils/camera/camera_diagnostics.h"
#include "utils/network/socket.h"


class QnByteArray;

// this class is designed just to be friendly with AV cams;( I doubd it can be usefull with smth else)
class CLSimpleTFTPClient
{
    // TODO: #Elric #enum
    enum {all_ok,  time_out};
    enum {blk_size=1450, double_blk_size=2904};

    // arecont vision cams supports 2 blk sizes.
    // I think second one is better 
    //1) you save cpu time, coz you call sendto function 2 times less ( huge savings on windows )
    //2) more friendly with wireless network ( it hates acks).

public:
    CLSimpleTFTPClient(const QString& host, unsigned int timeout, unsigned int retry);
    ~CLSimpleTFTPClient(){};

    const QString& getHostAddress() const { return m_hostAddress; }

    int read(const QString& fn, QnByteArray& data);
    const unsigned char* getLastPacket(int& size) const
    {
        size = m_last_packet_size;
        return m_last_packet;
    }
    CameraDiagnostics::Result prevIOResult() const;

private:
    int form_read_request(const QString& fn, char* buff);
    int form_ack(unsigned short blk, char* buff);

private:

    unsigned char m_last_packet[3100];
    unsigned int m_last_packet_size;

    int m_retry;
    QString m_hostAddress;
    int m_timeout;

    int m_status;
    int m_wish_blk_size;
    int m_curr_blk_size;

    std::auto_ptr<AbstractDatagramSocket> m_sock;
    CameraDiagnostics::Result m_prevResult;
    QString m_resolvedAddress;
};

#endif // ENABLE_ARECONT

#endif //__SIMPLE_TFTP_CLIENT__1117
