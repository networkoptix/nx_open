#ifndef __RTP_STREAM_PARSER_H
#define __RTP_STREAM_PARSER_H

#include <QIODevice>
#include "core/datapacket/mediadatapacket.h"



class RTPIODevice;

#pragma pack(push, 1)
typedef struct
{
    static const int RTP_HEADER_SIZE = 12;
    static const int RTP_VERSION = 2;
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    unsigned short   version:2;	/* packet type                */
    unsigned short   padding:1;		/* padding flag               */
    unsigned short   extension:1;		/* header extension flag      */
    unsigned short   CSRCCount:4;		/* CSRC count                 */
    unsigned short   marker:1;		/* marker bit                 */
    unsigned short   payloadType:7;		/* payload type               */
#else
    unsigned short   CSRCCount:4;		/* CSRC count                 */
    unsigned short   extension:1;		/* header extension flag      */
    unsigned short   padding:1;		/* padding flag               */
    unsigned short   version:2;	/* packet type                */
    unsigned short   payloadType:7;		/* payload type               */
    unsigned short   marker:1;		/* marker bit                 */
#endif
    quint16 sequence;		// sequence number
    quint32 timestamp;		// timestamp
    quint32 ssrc;		// synchronization source
    //quint32 csrc;		// synchronization source
    //quint32 csrc[1];	// optional CSRC list
} RtpHeader;
#pragma pack(pop)

class QnRtpStreamParser
{
public:
    QnRtpStreamParser();
    virtual void setSDPInfo(QList<QByteArray> sdpInfo) = 0;
    
    virtual bool processData(quint8* rtpBuffer, int readed, QList<QnAbstractMediaDataPtr>& result) = 0;

    virtual ~QnRtpStreamParser();
};

#endif