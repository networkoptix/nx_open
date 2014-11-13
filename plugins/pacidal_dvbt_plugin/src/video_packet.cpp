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

namespace ite
{
    DEFAULT_REF_COUNTER(VideoPacket)

    VideoPacket::VideoPacket( const uint8_t* data, unsigned size )
    :   m_refManager( this ),
        m_size( 0 ),
        m_time( 0 ),
        m_flags( 0 )
    {
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
    }

    void * VideoPacket::queryInterface( const nxpl::NX_GUID& interfaceID )
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
        if (interfaceID == nxpl::IID_PluginInterface)
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
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
