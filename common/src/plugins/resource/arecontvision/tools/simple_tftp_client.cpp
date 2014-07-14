#include "simple_tftp_client.h"

#ifdef ENABLE_ARECONT

#include "utils/network/socket.h"
#include "utils/common/log.h"
#include "utils/common/byte_array.h"
#include "utils/common/systemerror.h"
#include "core/dataprovider/abstract_streamdataprovider.h"

static const int SERVER_TFTP_PORT = 69;

using namespace std;

CLSimpleTFTPClient::CLSimpleTFTPClient(const QString& host, unsigned int timeout, unsigned int retry):
    m_retry(retry),
    m_hostAddress(host),
    m_timeout(timeout),
    m_resolvedAddress(resolveAddress(host).toString())
{
    m_sock.reset( SocketFactory::createDatagramSocket() );

    //m_wish_blk_size = double_blk_size;
    m_wish_blk_size  = blk_size;
    m_sock->setRecvTimeout(max(m_timeout,1000)); // minimum timeout is 1000 ms
    if (!m_sock->setDestAddr(m_resolvedAddress, SERVER_TFTP_PORT))
    {
        qWarning() << "CLSimpleTFTPClient::CLSimpleTFTPClient: setDestAddr() failed: " << SystemError::getLastOSErrorText();
    }
}

int CLSimpleTFTPClient::read( const QString& fn, QnByteArray& data)
{
    try
    {
        m_last_packet_size = 0;
        char buff_send[1000]; int len_send;
        unsigned char buff_recv[3100]; 
        int len_recv;
        int i, len = 0;

        m_sock->setDestAddr( m_resolvedAddress, SERVER_TFTP_PORT );

        len_send = form_read_request(fn, buff_send);

        QString temp_cam_addr;
        unsigned short cam_dst_port;

        int oa = 1;

        m_curr_blk_size = m_wish_blk_size;

        for (i = 0; i < m_retry; ++i)
        {
            m_prevResult = CameraDiagnostics::NoErrorResult();
            if (!m_sock->send(buff_send,len_send))
            {
                m_prevResult = CameraDiagnostics::IOErrorResult( SystemError::getLastOSErrorText() );
                return 0;
            }

            while(1)
            {
                m_prevResult = CameraDiagnostics::NoErrorResult();
                len_recv = m_sock->recvFrom(buff_recv, sizeof(buff_recv),temp_cam_addr, cam_dst_port);
                if (len_recv < 0)
                {
                    m_prevResult = CameraDiagnostics::IOErrorResult( SystemError::getLastOSErrorText() );
                    m_status = time_out; 
                    break;
                }
                m_sock->setDestAddr(m_resolvedAddress, cam_dst_port);

                if (len_recv<13) // unexpected answer
                    continue;

                if (buff_recv[0]==0 && buff_recv[1]==0x06)// this option ack
                {
                    // some times ( do not know why) cam responds with wrong blk size - very very rarely
                    if (m_wish_blk_size==double_blk_size && buff_recv[10]=='1')
                    {
                        //NX_LOG("unexpected packet size", cl_logWARNING);
                        m_curr_blk_size = blk_size;
                    }
                    else if (m_wish_blk_size==blk_size && buff_recv[10]=='2')
                    {
                        //NX_LOG("unexpected packet size", cl_logWARNING);
                        m_curr_blk_size = double_blk_size;
                    }

                    m_status = all_ok;
                    break;
                }
            }

            if (m_status==all_ok)
                break;
        }

        if (m_status!=all_ok)// no response
            return 0;

        // at this point we've got option ack on our request; => must send ack on each packet
        // tftp packets can come in any order like 1 2 3 8 7 5 6; this is unusual but possible on busy networks ( usually we will have 1234567...)
        // so any way we need to process any incoming block data: put data in a proper position and send ack
        // if we got last packet => done

        int blk_cam_sending = 0;
        int last_pack_number = 0;
        int data_size0= data.size();

        while(1)
        {

            len_send = form_ack(blk_cam_sending, buff_send);

            for (i = 0; i < m_retry; ++i)
            {

                //NX_LOG("sending... ", blk_cam_sending, cl_logWARNING);

                m_prevResult = CameraDiagnostics::NoErrorResult();
                if (!m_sock->send(buff_send,len_send))
                {
                    m_prevResult = CameraDiagnostics::IOErrorResult( SystemError::getLastOSErrorText() );
                    return 0;
                }

                //NX_LOG("send ", blk_cam_sending, cl_logWARNING);

                while(1)
                {
                    m_prevResult = CameraDiagnostics::NoErrorResult();
                    len_recv = m_sock->recv(buff_recv, sizeof(buff_recv), 0);

                    if (len_recv < 4)// unexpected answer or did not get anything
                    {
                        m_prevResult = CameraDiagnostics::IOErrorResult( SystemError::getLastOSErrorText() );
                        m_status = time_out;
                        break;
                    }

                    if (buff_recv[0] == 0 && buff_recv[1] == 3)// data block
                    {

                        blk_cam_sending = buff_recv[2]*256 + buff_recv[3];

                        //NX_LOG("got ", blk_cam_sending, cl_logWARNING);

                        int data_len = len_recv-4;

                        if (data_len<m_curr_blk_size)
                        {
                            memcpy(m_last_packet, buff_recv+4, data_len);
                            m_last_packet_size = data_len;
                            last_pack_number = blk_cam_sending;
                            goto LAST_PACKET;
                        }

                        data.writeAt((char*)buff_recv+4, data_len, (blk_cam_sending-1)*m_curr_blk_size + data_size0);

                        len+=data_len;
                        if (len>CL_MAX_DATASIZE)
                        {
                            NX_LOG("Image is too big!!", cl_logERROR);
                            m_status = time_out;
                            break;
                        }

                        m_status = all_ok; // need next packet
                        break;
                    }
                    else if (buff_recv[0]==0 && buff_recv[1]==0x06)// this option ack
                    {
                        if (oa==1) oa++; // arecont vision cam send min 2 ao; so first time we shoud ignore it
                        else
                        {
                            // this is 3 times we got option ack; need to resend ack0?
                            NX_LOG("this is 3 times we got option ack; need to resend ack0?", cl_logDEBUG1);

                            if (len_recv<13) // unexpected answer
                                continue;

                            // some times ( do not know why) cam responds with wrong blk size - very very rarely
                            if (m_wish_blk_size==double_blk_size && buff_recv[10]=='1')
                            {
                                //NX_LOG("unexpected packet size", cl_logWARNING);
                                m_curr_blk_size = blk_size;
                            }
                            else if (m_wish_blk_size==blk_size && buff_recv[10]=='2')
                            {
                                //NX_LOG("unexpected packet size", cl_logWARNING);
                                m_curr_blk_size = double_blk_size;
                            }

                            len_send = form_ack(0, buff_send); // need to send akc0 again
                            break;
                        }
                    }
                }

                if (m_status==all_ok)
                    break;

            }

            if (m_status == time_out)
                return 0;

                                                                                                 }

LAST_PACKET:

        // need to send ack to last pack;
        len_send = form_ack(last_pack_number, buff_send);
        m_prevResult = CameraDiagnostics::NoErrorResult();
        if( !m_sock->send(buff_send,len_send) ) // send ack
            m_prevResult = CameraDiagnostics::IOErrorResult( SystemError::getLastOSErrorText() );

        return len;
    } catch (...)
    {
        // Socket exception occured.
        return 0;
    }
}

CameraDiagnostics::Result CLSimpleTFTPClient::prevIOResult() const
{
    return m_prevResult;
}

int CLSimpleTFTPClient::form_read_request(const QString& fn, char* buff)
{
    buff[0] = 0; buff[1] = 1; // read request;
    int len = 2;
    int req_len = fn.length();

    memcpy(buff+len, fn.toLatin1(), req_len);    len+=req_len;
    buff[len] = 0;    len++;

    memcpy(buff+len, "netascii", 8); len+=8;
    buff[len] = 0;    len++;

    memcpy(buff+len, "blksize", 7); len+=7;
    buff[len] = 0;    len++;

    if (m_wish_blk_size == double_blk_size)
        memcpy(buff+len, "2904", 4); 
    else
        memcpy(buff+len, "1450", 4); 

    len+=4;
    buff[len] = 0;    len++;

    return len;

}

int CLSimpleTFTPClient::form_ack(unsigned short blk, char* buff)
{
    buff[0] = 0; buff[1] = 0x04; 
    buff[2] = blk>>8; buff[3] = blk&0xff;

    return 4;
}

#endif // ENABLE_ARECONT

