#ifndef __PES_PACKET_H
#define __PES_PACKET_H

#include <QtCore/QObject>

#include <nx/utils/log/assert.h>

const static quint64 MAX_PTS = 8589934592ll - 1;
const static quint8 PES_DATA_ALIGNMENT = 4;

static qint64 get_pts(const quint8 *p)
{
    qint64 pts;
    int val;
    pts = (qint64)((p[0] >> 1) & 0x07) << 30;
    val = (p[1] << 8) | p[2];
    pts |= (qint64)(val >> 1) << 15;
    val = (p[3] << 8) | p[4];
    pts |= (qint64)(val >> 1);
    return pts;
}

static void set_pts_int(quint8 *p, qint64 pts, int prefix)
{
    p[0] = prefix + (quint8) (((pts >> 30) &0x07)<<1) + 1;
    quint16 val = (quint16) (((pts>>15)&0x7fff)<<1) + 1;
    p[1] = val >> 8;
    p[2] = val & 0xff;

    val = (quint16) ((pts&0x7fff)<<1) + 1;
    p[3] = val >> 8;
    p[4] = val & 0xff;
}

struct PESPacket {
    // value 0x00 0001.
    quint8 startCode0;
    quint8 startCode1;
    quint8 startCode2;

    /*
    1011 1100 - Reserved stream.
    1011 1101 - Private Stream 1.
    1011 1110 - Padding Stream.
    1011 1111 - Private Stream 2.

    110x xxxx - MPEG Audio Stream Number xxxxx.
    1110 xxxx - MPEG Video Stream Number xxxx.
    1111 0000 ECM stream
    1111 0001.EMM stream
    1111 0010 DSM CC stream
    1111 0011 MHEG stream
    1111 0100 - 1111 1000 ITU-T Rec. H.222.1 type A - type E
    1111 1001 ancillary stream
    1111 1010 - 1111 1110 reserved data stream
    1111 1111 program stream directory
    */
    quint8 m_streamID;

    //quint16 m_pesPacketLen;
    quint8 m_pesPacketLenHi;
    quint8 m_pesPacketLenLo;

    quint8 flagsHi;
    quint8 flagsLo;

    /*
    //1 - Original;
    //0 - Copy.
    int ooc:1;

    //1 - Copyrighted.
    //0 - Not defined.
    int cy:1;

    //Indicates the nature of alignment of the first start code occurring in the payload.
    //The type of data in the payload is indicated by the data_stream_alignment_descriptor.
    //1 - Aligned;
    //0 - No indication of alignment. (Must be aligned for video.)

    int dai:1;

    //Indicates the priority of this packet with respect to other packets: 1=high priority; 0=no priority.
    int pesp:1;

    //00 - Not scrambled.
    //01 - User defined.
    //10 - User defined.
    //11 - User defined.
    int pessc:2;

    int reserved:2; // const value "10"

    //The flag is set as necessary to indicate that extension flags are set in the PES header. Its use includes support of private data.
    //1 - Field is present.
    //0 - Field is not present.
    int ext:1;

    //Indicates the presence of a CRC field in the PES packet.
    int crc:1;

    //Indicates the presence of the additional_copy_info field.
    //1 - Field is present.
    //0 - Field is not present.
    int aci:1;

    //Indicates the presence of an 8 bit field describing the DSM (Digital Storage Media) operating mode:
    //1 - Field is present.
    //0 - Field is not present. (For broadcasting purposes, set = 0.)
    int tm:1;

    //Indicates whether the Elementary Stream Rate field is present in the PES header
    int rate:1;

    //Indicates whether the Elementary Stream Clock Reference field is present in the PES header
    int escr:1;

    //00 - Neither PTS or DTS are present in the header.
    //1x - PTS field is present.
    //11 - Both PTS or DTS are present in the header.
    int tsf: 2;
    */

    quint8 m_pesHeaderLen;

    void setStreamID(quint8 streamID)
    {
        startCode0 = 0;
        startCode1 = 0;
        startCode2 = 1;
        m_streamID = streamID;
    }

    quint16 getPacketLength() {
        quint16 packetLen = (m_pesPacketLenHi << 8) + m_pesPacketLenLo;
        return packetLen ? packetLen + 6 : 0;
    }

    void setPacketLength(quint16 len)
    {
        if (len != 0) {
            NX_ASSERT(len >= 6);
            len -= 6;
        }
        m_pesPacketLenHi = len >> 8;
        m_pesPacketLenLo = len & 0xff;
    }

    quint8 getHeaderLength() {
        return m_pesHeaderLen + 9;
    }

    void setHeaderLength(quint8 val) {
        m_pesHeaderLen = val - 9;
    }

    void resetFlags() {
        flagsHi = 0x80;
        flagsLo = 0;
    }

    bool hasPts() const { return (flagsLo & 0x80) == 0x80; }
    bool hasDts() const { return (flagsLo & 0x40) == 0x40; }

    quint64 getPts() const { return get_pts((quint8*) this + HEADER_SIZE); }
    quint64 getDts() const { return get_pts((quint8*) this + HEADER_SIZE + PTS_SIZE); }

    void setPts(quint64 pts) {
        set_pts_int((quint8*)this + HEADER_SIZE, pts, 0x20);
        flagsLo = (flagsLo & 0x3f) | 0x80;
    }
    void setPtsAndDts(quint64 pts, quint64 dts) {
        set_pts_int((quint8*)this + HEADER_SIZE, pts, 0x30);
        set_pts_int((quint8*)this + HEADER_SIZE + PTS_SIZE, dts, 0x10);
        flagsLo = flagsLo | 0xc0;
    }
    void serialize(quint8 streamID);
    void serialize(quint64 pts, quint8 streamID);
    void serialize(quint64 pts, quint64 dts, quint8 streamID);
    bool startCodeExists() const { return startCode0 == 0 && startCode1 == 0 && startCode2 == 1; }
public:
    const static int HEADER_SIZE = 9;
    const static int PTS_SIZE = 5;

};

#endif
