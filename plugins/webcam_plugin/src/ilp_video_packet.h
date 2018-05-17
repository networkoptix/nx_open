/**********************************************************
* 05 sep 2013
* akolesnikov
***********************************************************/

#ifndef ILP_VIDEO_PACKET_H
#define ILP_VIDEO_PACKET_H

#include <camera/camera_plugin.h>

#include <plugins/plugin_tools.h>


class CyclicAllocator;

namespace nx {
namespace webcam_plugin {

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

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

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

} // namespace webcam_plugin
} // namespace nx

#endif  //ILP_VIDEO_PACKET_H
