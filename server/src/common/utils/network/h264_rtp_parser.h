#ifndef __H264_RTP_PARSER_H
#define __H264_RTP_PARSER_H

#include <QByteArray>
#include <QMap>
#include "common/nalUnits.h"
#include "common/bytearray.h"
#include "rtp_stream_parser.h"



class CLH264RtpParser: public CLRtpStreamParser
{
public:
    CLH264RtpParser(RTPIODevice* input);
    virtual CLAbstractMediaData* getNextData();
    virtual ~CLH264RtpParser();
    virtual void setSDPInfo(const QByteArray& data);
private:
    //QByteArray m_sliceBuffer;
    int m_readBufferActualSize;
    QMap <int, QByteArray> m_allNonSliceNal;
    QByteArray m_sdpSpsPps;
    SPSUnit m_sps;
    int m_frequency;
    int m_rtpChannel;
    bool m_builtinSpsFound;
    bool m_builtinPpsFound;
    quint64 m_timeCycles;
    quint64 m_timeCycleValue;
    quint64 m_lastTimeStamp;
    quint16 m_firstSeqNum;
    quint16 m_packetPerNal;
private:
    void serializeSpsPps(CLByteArray& dst);
    void decodeSpsInfo(const QByteArray& data);
};

#endif
