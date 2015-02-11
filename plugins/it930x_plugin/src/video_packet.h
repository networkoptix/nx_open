#ifndef ITE_VIDEO_PACKET_H
#define ITE_VIDEO_PACKET_H

#include <memory>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "ref_counter.h"

namespace ite
{
    //!
    class VideoPacket : public nxcip::VideoDataPacket
    {
        DEF_REF_COUNTER

    public:
        VideoPacket(const uint8_t* data, unsigned size);
        virtual ~VideoPacket();

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

        int64_t pts() const { return m_pts; }
        void setPTS(int64_t pts) { m_pts = pts; }

        void setTime(uint64_t ts) { m_time = ts; }
        void setKeyFlag() { m_flags |= MediaDataPacket::fKeyPacket; }
        void setLowQualityFlag() { m_flags |= MediaDataPacket::fLowQuality; }

        bool isKey() const { return m_flags & MediaDataPacket::fKeyPacket; }

        void swap(VideoPacket& vp)
        {
            using std::swap;

            m_data.swap(vp.m_data);
            swap(m_size, vp.m_size);
            swap(m_pts, vp.m_pts);
            swap(m_time, vp.m_time);
            swap(m_flags, vp.m_flags);
        }

    private:
        std::shared_ptr<uint8_t> m_data;
        size_t m_size;
        int64_t m_pts;
        nxcip::UsecUTCTimestamp m_time;
        unsigned m_flags;
    };
}

#endif // ITE_VIDEO_PACKET_H
