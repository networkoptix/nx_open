/**********************************************************
* 09 dec 2013
* akolesnikov
***********************************************************/

#include "isd_audio_packet.h"


AudioData::AudioData(
    size_t reserveSize,
    nxcip::CompressionType audioCodec )
:
    m_data( nullptr ),
    m_dataSize( 0 ),
    m_capacity( 0 ),
    m_audioCodec( audioCodec ),
    m_timestamp( -1 )
{
    m_data = new uint8_t[reserveSize];
    if( !m_data )
        return;
    m_capacity = reserveSize;
}

AudioData::~AudioData()
{
    if( m_data )
    {
        delete[] m_data;
        m_data = nullptr;
    }
}

const uint8_t* AudioData::data() const
{
    return m_data;
}

size_t AudioData::dataSize() const
{
    return m_dataSize;
}

nxcip::CompressionType AudioData::codecType() const
{
    return m_audioCodec;
}

size_t AudioData::capacity() const
{
    return m_capacity;
}

uint8_t* AudioData::data()
{
    return m_data;
}

void AudioData::setDataSize( size_t _size )
{
    m_dataSize = _size;
}

void AudioData::setTimestamp( nxcip::UsecUTCTimestamp timestamp )
{
    m_timestamp = timestamp;
}

nxcip::UsecUTCTimestamp AudioData::getTimestamp() const
{
    return m_timestamp;
}



ISDAudioPacket::ISDAudioPacket( int _channelNumber )
:
    m_refManager( this ),
    m_channelNumber( _channelNumber )
{
}

ISDAudioPacket::~ISDAudioPacket()
{
}

//!Implementation of nxpl::PluginInterface::queryInterface
void* ISDAudioPacket::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_MediaDataPacket, sizeof(nxcip::IID_MediaDataPacket) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::MediaDataPacket*>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

//!Implementaion of nxpl::PluginInterface::addRef
unsigned int ISDAudioPacket::addRef()
{
    return m_refManager.addRef();
}

//!Implementaion of nxpl::PluginInterface::releaseRef
unsigned int ISDAudioPacket::releaseRef()
{
    return m_refManager.releaseRef();
}

//!Implementation of nxpl::MediaDataPacket::isKeyFrame
nxcip::UsecUTCTimestamp ISDAudioPacket::timestamp() const
{
    return m_audioData->getTimestamp();
}

//!Implementation of nxpl::MediaDataPacket::type
nxcip::DataPacketType ISDAudioPacket::type() const
{
    return nxcip::dptAudio;
}

//!Implementation of nxpl::MediaDataPacket::data
const void* ISDAudioPacket::data() const
{
    return m_audioData->data();
}

//!Implementation of nxpl::MediaDataPacket::dataSize
unsigned int ISDAudioPacket::dataSize() const
{
    return m_audioData->dataSize();
}

//!Implementation of nxpl::MediaDataPacket::channelNumber
unsigned int ISDAudioPacket::channelNumber() const
{
    return m_channelNumber;
}

//!Implementation of nxpl::MediaDataPacket::codecType
nxcip::CompressionType ISDAudioPacket::codecType() const
{
    return m_audioData->codecType();
}

//!Implementation of nxpl::MediaDataPacket::flags
unsigned int ISDAudioPacket::flags() const
{
    return 0;
}

//!Implementation of nxpl::MediaDataPacket::cSeq
unsigned int ISDAudioPacket::cSeq() const
{
    return 0;
}
