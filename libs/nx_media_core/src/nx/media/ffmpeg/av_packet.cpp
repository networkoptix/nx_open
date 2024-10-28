// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "av_packet.h"

extern "C" {
#include <libavcodec/packet.h>
} // extern "C"

#include <nx/media/media_data_packet.h>

namespace nx::media::ffmpeg {

AvPacket::AvPacket(uint8_t* data, int size)
{
    m_packet = av_packet_alloc();
    m_packet->data = data;
    m_packet->size = size;
}

AvPacket::AvPacket(const QnAbstractMediaData* data)
{
    // TODO: Do not alloc packet for flushing after checking is it still needed passing last pts
    // to decoder when flush (see code 'packet->pts = packet->dts = d->lastPts;' in decoder classes).
    m_packet = av_packet_alloc();

    if (!data)
        return;

    m_packet->data = (unsigned char*)data->data();
    m_packet->size = static_cast<int>(data->dataSize());
    m_packet->dts = m_packet->pts = data->timestamp;
    if (data->flags & QnAbstractMediaData::MediaFlags_AVKey)
        m_packet->flags = AV_PKT_FLAG_KEY;

    // TODO: Check is it really necessary
    // It's already guaranteed by nx::utils::ByteArray that there is an extra space reserved. We must
    // fill the padding bytes according to ffmpeg documentation.
    if (m_packet->data)
        memset(m_packet->data + m_packet->size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
}

AvPacket::~AvPacket()
{
    av_packet_free(&m_packet);
}

} // namespace nx::media::ffmpeg
