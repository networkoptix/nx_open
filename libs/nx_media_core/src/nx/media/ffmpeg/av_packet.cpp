// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "av_packet.h"

extern "C" {
#include <libavcodec/defs.h>
#include <libavcodec/packet.h>
} // extern "C"

#include <cstring>

#include <nx/media/media_data_packet.h>
#include <nx/utils/log/log.h>

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

    const int size = static_cast<int>(data->dataSize());

    // Ffmpeg documentation requires AV_INPUT_BUFFER_PADDING_SIZE zero bytes after the data,
    // because some optimized bitstream readers read 32 or 64 bits at once and could read over the
    // end of the buffer. Packets which store the data in nx::utils::ByteArray provide such a
    // padding, the others have to be copied: the memory behind their buffer is not ours to write.
    if (!data->data() || size <= 0 || data->paddingSize() >= AV_INPUT_BUFFER_PADDING_SIZE)
    {
        m_packet->data = (unsigned char*) data->data();
        m_packet->size = size;
    }
    else if (av_new_packet(m_packet, size) == 0) //< Allocates and zeroes the padding as well.
    {
        memcpy(m_packet->data, data->data(), size);
    }
    else
    {
        NX_WARNING(this, "Failed to allocate a packet of %1 bytes", size);
    }

    m_packet->dts = m_packet->pts = data->timestamp;
    if (data->isKeyFrame())
        m_packet->flags = AV_PKT_FLAG_KEY;
}

AvPacket::~AvPacket()
{
    av_packet_free(&m_packet);
}

} // namespace nx::media::ffmpeg
