#ifndef LIBAV_VIDEO_PACKET_H
#define LIBAV_VIDEO_PACKET_H

#include <memory>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "ref_counter.h"

namespace pacidal
{
    //!
    class VideoPacket
    :
        public nxcip::VideoDataPacket
    {
        DEF_REF_COUNTER

    public:
        VideoPacket()
        :
            m_refManager( this ),
            m_data( nullptr ),
            m_size( 0 )
        {
        }

        VideoPacket(const uint8_t* data, unsigned size);
        virtual ~VideoPacket();

        //!Implementation of nxpl::MediaDataPacket::isKeyFrame
        virtual nxcip::UsecUTCTimestamp timestamp() const override;
        //!Implementation of nxpl::MediaDataPacket::type
        virtual nxcip::DataPacketType type() const override;
        //!Implementation of nxpl::MediaDataPacket::data
        virtual const void* data() const override;
        //!Implementation of nxpl::MediaDataPacket::dataSize
        virtual unsigned int dataSize() const override;
        //!Implementation of nxpl::MediaDataPacket::channelNumber
        virtual unsigned int channelNumber() const override;
        //!Implementation of nxpl::MediaDataPacket::codecType
        virtual nxcip::CompressionType codecType() const override;
        //!Implementation of nxpl::MediaDataPacket::flags
        virtual unsigned int flags() const override;
        //!Implementation of nxpl::MediaDataPacket::cSeq
        virtual unsigned int cSeq() const override;

        //!Implementation of nxpl::VideoDataPacket::getMotionData
        virtual nxcip::Picture* getMotionData() const override;

        void setTime(unsigned ts) { m_time = ts; }
        void setKeyFlag() { m_flags |= MediaDataPacket::fKeyPacket; }
        void setLowQualityFlag() { m_flags |= MediaDataPacket::fLowQuality; }

        bool isKey() const { return m_flags & MediaDataPacket::fKeyPacket; }

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

#endif  //LIBAV_VIDEO_PACKET_H
