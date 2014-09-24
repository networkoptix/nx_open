/**********************************************************
* 09 dec 2013
* akolesnikov
***********************************************************/

#ifndef ISD_AUDIO_PACKET_H
#define ISD_AUDIO_PACKET_H

#include <stdint.h>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>


class ISDAudioPacket
:
    public nxcip::MediaDataPacket
{
public:
    ISDAudioPacket(
        int _channelNumber,
        nxcip::UsecUTCTimestamp _timestamp,
        nxcip::CompressionType _codecType );
    virtual ~ISDAudioPacket();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementaion of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementaion of nxpl::PluginInterface::releaseRef
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

    void* data();
    void reserve( unsigned int _capacity );
    unsigned int capacity() const;
    void setDataSize( unsigned int _dataSize );

private:
    nxpt::CommonRefManager m_refManager;
    const int m_channelNumber;
    nxcip::UsecUTCTimestamp m_timestamp;
    nxcip::CompressionType m_codecType;

    uint8_t* m_data;
    size_t m_capacity;
    size_t m_dataSize;
};


#endif  //ISD_AUDIO_PACKET_H
