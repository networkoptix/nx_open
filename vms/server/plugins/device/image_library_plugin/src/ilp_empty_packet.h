/**********************************************************
* 16 sep 2013
* akolesnikov
***********************************************************/

#ifndef ELP_EMPTY_PACKET_H
#define ELP_EMPTY_PACKET_H

#include <camera/camera_plugin.h>

#include <plugins/plugin_tools.h>


class ILPEmptyPacket
:
    public nxcip::VideoDataPacket
{
public:
    ILPEmptyPacket(
        int channelNumber,
        nxcip::UsecUTCTimestamp _timestamp,
        unsigned int flags,
        unsigned int cSeq );
    virtual ~ILPEmptyPacket();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual int addRef() const override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual int releaseRef() const override;

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

private:
    nxpt::CommonRefManager m_refManager;
    const int m_channelNumber;
    nxcip::UsecUTCTimestamp m_timestamp;
    unsigned int m_flags;
    unsigned int m_cSeq;
};

#endif  //ELP_EMPTY_PACKET_H
