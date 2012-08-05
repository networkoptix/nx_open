#include "rtsp_encoder.h"

void QnRtspEncoder::setMediaData(QnAbstractMediaDataPtr media)
{
    m_sdpMediaPacket = media;
}
