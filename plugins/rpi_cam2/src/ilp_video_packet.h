#pragma once

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

class CyclicAllocator;

namespace nx {
namespace rpi_cam2 {

class ILPVideoPacket
    :
    public nxcip::VideoDataPacket{
public:
    ILPVideoPacket(
        CyclicAllocator* const allocator,
        int channelNumber,
        nxcip::UsecUTCTimestamp _timestamp,
        unsigned int flags,
        unsigned int cSeq);
    virtual ~ILPVideoPacket();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;
    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;

    virtual nxcip::UsecUTCTimestamp timestamp() const override;
    virtual nxcip::DataPacketType type() const override;
    virtual const void* data() const override;
    virtual unsigned int dataSize() const override;
    virtual unsigned int channelNumber() const override;
    virtual nxcip::CompressionType codecType() const override;
    virtual unsigned int flags() const override;
    virtual unsigned int cSeq() const override;
    virtual nxcip::Picture* getMotionData() const override;

    void setCodecType(nxcip::CompressionType codecType);

    /*!
        \note Does keep contents of current buffer
    */
    void resizeBuffer(size_t bufSize);
    void* data();

private:
    nxpt::CommonRefManager m_refManager;
    CyclicAllocator* const m_allocator;
    const int m_channelNumber;
    nxcip::UsecUTCTimestamp m_timestamp;
    void* m_buffer;
    size_t m_bufSize;
    unsigned int m_flags;
    unsigned int m_cSeq;
    nxcip::CompressionType m_codecType;
};

} // namespace rpi_cam2
} // namespace nx