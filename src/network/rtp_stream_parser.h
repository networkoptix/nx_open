#ifndef __RTP_STREAM_PARSER_H
#define __RTP_STREAM_PARSER_H

#include <QIODevice>

#include "data/mediadata.h"
class RTPIODevice;

#pragma pack(push, 1)
typedef struct
{
    static const int RTP_HEADER_SIZE = 12;
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



class CLRtpStreamParser
{
public:
    CLRtpStreamParser(RTPIODevice* input);
    virtual void setSDPInfo(const QByteArray& data);
    virtual CLAbstractMediaData* getNextData() = 0;
    virtual ~CLRtpStreamParser();
protected:
    RTPIODevice* m_input;
    QByteArray m_sdp;
};

#endif