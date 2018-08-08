#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include "nx/streaming/media_data_packet.h"
#include "utils/common/byte_array.h"

/*
* Base class for RTSP codec. Used for encode data only
*/

class QnRtspEncoder
{
public:
    QnRtspEncoder();
    virtual ~QnRtspEncoder() {}

    virtual QString getSdpMedia(bool isVideo, int trackId) = 0;

    /*
    * Set media packet to encode
    */
    virtual void setDataPacket(QnConstAbstractMediaDataPtr media) = 0;

    /*
    * Function MUST write encoded data packet to sendBuffer. sendBuffer may contain some data before function call (TCP RTSP header)
    * So, function MUST add data only without clearing buffer
    * return true if some data are writed or false if no more data
    */
    virtual bool getNextPacket(QnByteArray& sendBuffer) = 0;

    virtual void init() = 0;

    /*
    * Functions for filling RTP header
    */
    virtual quint32 getSSRC() = 0;
    virtual bool getRtpMarker() = 0;
    virtual quint32 getFrequency() = 0;
    virtual quint8 getPayloadType() = 0;
    virtual QString getName() = 0;

    /*
    * Return true if codec can produce RTP header
    */
    virtual bool isRtpHeaderExists() const = 0;

    static void buildRTPHeader(char* buffer, quint32 ssrc, int markerBit, quint32 timestamp, quint8 payloadType, quint16 sequence);
};

using QnRtspEncoderPtr = std::shared_ptr<QnRtspEncoder>;

#endif // ENABLE_DATA_PROVIDERS
