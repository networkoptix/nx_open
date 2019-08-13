// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <cstring>
#include <cstdlib>

#include <plugins/plugin_tools.h>
#include <nx/kit/utils.h>

#include "video_packet.h"

namespace rpi_cam
{
    VideoPacket::VideoPacket(const uint8_t * data, size_t size, uint64_t ts, unsigned flags)
    :   m_size(0),
        m_time(ts),
        m_flags(flags)
    {
        typedef void FreeAlignedFunc(void*); //< Needed to choose the proper overloaded version.
        if (data)
        {
            m_data = std::shared_ptr<uint8_t>((uint8_t*)
                nx::kit::utils::mallocAligned(
                    size + nxcip::MEDIA_PACKET_BUFFER_PADDING_SIZE,
                    nxcip::MEDIA_DATA_BUFFER_ALIGNMENT),
                (FreeAlignedFunc*) nx::kit::utils::freeAligned);
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
        return nxcip::AV_CODEC_ID_H264;
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
