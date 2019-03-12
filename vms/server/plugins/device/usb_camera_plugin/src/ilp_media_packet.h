#pragma once

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

class CyclicAllocator;

namespace nx {
namespace usb_cam {

class ILPMediaPacket: public nxcip::VideoDataPacket {
public:
    ILPMediaPacket(
        CyclicAllocator* const allocator,
        int channelNumber,
        nxcip::DataPacketType dataType,
        nxcip::CompressionType compressionType,
        nxcip::UsecUTCTimestamp timestamp,
        unsigned int flags,
        unsigned int cSeq);
    virtual ~ILPMediaPacket();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;
    virtual int addRef() const override;
    virtual int releaseRef() const override;

    virtual nxcip::UsecUTCTimestamp timestamp() const override;
    virtual nxcip::DataPacketType type() const override;
    virtual const void* data() const override;
    virtual unsigned int dataSize() const override;
    virtual unsigned int channelNumber() const override;
    virtual nxcip::CompressionType codecType() const override;
    virtual unsigned int flags() const override;
    virtual unsigned int cSeq() const override;
    virtual nxcip::Picture* getMotionData() const override;

    void resizeBuffer(size_t bufSize);
    void* data();

private:
    nxpt::CommonRefManager m_refManager;
    CyclicAllocator* const m_allocator;
    const int m_channelNumber = 0;
    nxcip::DataPacketType m_dataType = nxcip::DataPacketType::dptEmpty;
    nxcip::CompressionType m_compressionType = nxcip::AV_CODEC_ID_NONE;
    nxcip::UsecUTCTimestamp m_timestamp = 0;
    unsigned int m_flags = 0;
    unsigned int m_cSeq = 0;
    void* m_buffer = nullptr;
    size_t m_bufSize = 0;
};

} // namespace usb_cam
} // namespace nx
