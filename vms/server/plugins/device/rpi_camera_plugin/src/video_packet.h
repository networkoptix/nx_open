// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef RPI_VIDEO_PACKET_H
#define RPI_VIDEO_PACKET_H

#include <memory>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "ref_counter.h"

namespace rpi_cam
{
    //!
    class VideoPacket : public DefaultRefCounter<nxcip::VideoDataPacket>
    {
    public:
        VideoPacket()
        :   m_data( nullptr ),
            m_size( 0 )
        {}

        VideoPacket(const uint8_t * data, size_t size, uint64_t ts, unsigned flags);
        virtual ~VideoPacket();

        // nxpl::PluginInterface

        virtual void * queryInterface( const nxpl::NX_GUID& interfaceID ) override;

        // nxpl::MediaDataPacket

        virtual nxcip::UsecUTCTimestamp timestamp() const override;
        virtual nxcip::DataPacketType type() const override;
        virtual const void* data() const override;
        virtual unsigned int dataSize() const override;
        virtual unsigned int channelNumber() const override;
        virtual nxcip::CompressionType codecType() const override;
        virtual unsigned int flags() const override;
        virtual unsigned int cSeq() const override;
        virtual nxcip::Picture* getMotionData() const override;

        //

        void swap(VideoPacket& vp)
        {
            using std::swap;

            m_data.swap(vp.m_data);
            swap(m_size, vp.m_size);
            swap(m_time, vp.m_time);
            swap(m_flags, vp.m_flags);
        }

    private:
        std::shared_ptr<uint8_t> m_data;
        size_t m_size;
        nxcip::UsecUTCTimestamp m_time;
        unsigned m_flags;
    };
}

#endif
