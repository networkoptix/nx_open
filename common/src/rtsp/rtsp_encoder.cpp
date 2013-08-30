#include "rtsp_encoder.h"

QnRtspEncoder::QnRtspEncoder()
{
}

void QnRtspEncoder::setMediaData(QnAbstractMediaDataPtr media)
{
    m_sdpMediaPacket = media;
}
