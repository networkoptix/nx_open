/**********************************************************
* 09 dec 2013
* akolesnikov
***********************************************************/

#ifndef ISD_AUDIO_PACKET_H
#define ISD_AUDIO_PACKET_H

#include <memory>
#include <stdint.h>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>


class AudioData
{
public:
    AudioData(
        size_t reserveSize,
        nxcip::CompressionType audioCodec );
    ~AudioData();

    const uint8_t* data() const;
    size_t dataSize() const;
    nxcip::CompressionType codecType() const;

    size_t capacity() const;
    uint8_t* data();
    void setDataSize( size_t _size );
    void setTimestamp( nxcip::UsecUTCTimestamp timestamp );
    nxcip::UsecUTCTimestamp getTimestamp() const;

private:
    uint8_t* m_data;
    size_t m_dataSize;
    size_t m_capacity;
    nxcip::CompressionType m_audioCodec;
    nxcip::UsecUTCTimestamp m_timestamp;
};

class ISDAudioPacket
:
    public nxcip::MediaDataPacket
{
public:
    ISDAudioPacket( int _channelNumber );
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

    template<class T>
    void setData( T&& audioData )
    {
        m_audioData = std::forward<T>(audioData);
    }

private:
    nxpt::CommonRefManager m_refManager;
    const int m_channelNumber;
    nxcip::CompressionType m_codecType;

    std::shared_ptr<AudioData> m_audioData;
};


#endif  //ISD_AUDIO_PACKET_H
