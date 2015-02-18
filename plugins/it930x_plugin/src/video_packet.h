#ifndef ITE_VIDEO_PACKET_H
#define ITE_VIDEO_PACKET_H

#include <memory>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "ref_counter.h"

namespace ite
{
    //!
    class VideoPacket : public nxcip::VideoDataPacket, public ObjectCounter<VideoPacket>
    {
        DEF_REF_COUNTER

    public:
        VideoPacket(const uint8_t * data, unsigned size, uint64_t ts);
        virtual ~VideoPacket();

        // nxpl::VideoDataPacket

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

        void setFlag(MediaDataPacket::Flags f) { m_flags |= f; }

    private:
        void * m_data;
        size_t m_size;
        nxcip::UsecUTCTimestamp m_time;
        unsigned m_flags;
    };
}

#endif // ITE_VIDEO_PACKET_H
