#include <algorithm>
#include <cstring>
#include <cstdlib>

#include <plugins/plugin_tools.h>

#include "video_packet.h"

namespace
{
    static void freeAlignedX( void* ptr)
    {
        nxpt::freeAligned( ptr );
    }
}

namespace rpi_cam
{
    DEFAULT_REF_COUNTER(VideoPacket)

    VideoPacket::VideoPacket(const uint8_t * data, size_t size, uint64_t ts, unsigned flags)
    :   m_refManager(this),
        m_size(0),
        m_time(ts),
        m_flags(flags)
    {
        //printf("[camera] VideoPacket()\n");

        if (data)
        {
            m_data = std::shared_ptr<uint8_t>(
                (uint8_t*)nxpt::mallocAligned( size + nxcip::MEDIA_PACKET_BUFFER_PADDING_SIZE, nxcip::MEDIA_DATA_BUFFER_ALIGNMENT), freeAlignedX );
            memcpy( m_data.get(), data, size );
            memset( m_data.get() + size, 0, nxcip::MEDIA_PACKET_BUFFER_PADDING_SIZE );
            m_size = size;
        }
    }

    VideoPacket::~VideoPacket()
    {
        //printf("[camera] ~VideoPacket()\n");
    }

    // TODO
    void* VideoPacket::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if (interfaceID == nxcip::IID_VideoDataPacket)
        {
            addRef();
            return this;
        }
        if (interfaceID == nxcip::IID_MediaDataPacket)
        {
            addRef();
            return static_cast<nxcip::MediaDataPacket*>(this);
        }

        return NULL;
    }

    nxcip::UsecUTCTimestamp VideoPacket::timestamp() const
    {
        return m_time;
    }

    nxcip::DataPacketType VideoPacket::type() const
    {
        return nxcip::dptVideo;
    }

    const void* VideoPacket::data() const
    {
        return m_data.get();
    }

    unsigned int VideoPacket::dataSize() const
    {
        return m_size;
    }

    unsigned int VideoPacket::channelNumber() const
    {
        return 0;
    }

    nxcip::CompressionType VideoPacket::codecType() const
    {
        return nxcip::CODEC_ID_H264;
    }

    unsigned int VideoPacket::flags() const
    {
        return m_flags;
    }

    unsigned int VideoPacket::cSeq() const
    {
        return 0;
    }

    nxcip::Picture* VideoPacket::getMotionData() const
    {
        return nullptr;
    }
}
