#include "rtsp_encoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <utils/network/rtp_stream_parser.h>

QnRtspEncoder::QnRtspEncoder()
{
}

void QnRtspEncoder::setMediaData(QnConstAbstractMediaDataPtr media)
{
    m_sdpMediaPacket = media;
}

void QnRtspEncoder::buildRTPHeader(char* buffer, quint32 ssrc, int markerBit, quint32 timestamp, quint8 payloadType, quint16 sequence)
{
    RtpHeader* rtp = (RtpHeader*) buffer;
    rtp->version = RtpHeader::RTP_VERSION;
    rtp->padding = 0;
    rtp->extension = 0;
    rtp->CSRCCount = 0;
    rtp->marker  =  markerBit;
    rtp->payloadType = payloadType;
    rtp->sequence = htons(sequence);
    //rtp->timestamp = htonl(m_timer.elapsed());
    rtp->timestamp = htonl(timestamp);
    rtp->ssrc = htonl(ssrc); // source ID
}

#endif // ENABLE_DATA_PROVIDERS
