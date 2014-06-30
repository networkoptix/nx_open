/**********************************************************
* 09 dec 2013
* akolesnikov
***********************************************************/

#include "isd_audio_packet.h"


ISDAudioPacket::ISDAudioPacket(
    int _channelNumber,
    nxcip::UsecUTCTimestamp _timestamp,
    nxcip::CompressionType _codecType )
:
    m_refManager( this ),
    m_channelNumber( _channelNumber ),
    m_timestamp( _timestamp ),
    m_codecType( _codecType ),
    m_data( 0 ),
    m_capacity( 0 ),
    m_dataSize( 0 )
{
}

ISDAudioPacket::~ISDAudioPacket()
{
    nxpt::freeAligned( m_data );
    m_data = nullptr;
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
    return m_timestamp;
}

//!Implementation of nxpl::MediaDataPacket::type
nxcip::DataPacketType ISDAudioPacket::type() const
{
    return nxcip::dptAudio;
}

//!Implementation of nxpl::MediaDataPacket::data
const void* ISDAudioPacket::data() const
{
    return m_data;
}

//!Implementation of nxpl::MediaDataPacket::dataSize
unsigned int ISDAudioPacket::dataSize() const
{
    return m_dataSize;
}

//!Implementation of nxpl::MediaDataPacket::channelNumber
unsigned int ISDAudioPacket::channelNumber() const
{
    return m_channelNumber;
}

//!Implementation of nxpl::MediaDataPacket::codecType
nxcip::CompressionType ISDAudioPacket::codecType() const
{
    return m_codecType;
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

void* ISDAudioPacket::data()
{
    return m_data;
}

void ISDAudioPacket::reserve( unsigned int _capacity )
{
    if( m_data )
        nxpt::freeAligned( m_data );

    m_capacity = _capacity;
    m_data = (uint8_t*)nxpt::mallocAligned( _capacity + nxcip::MEDIA_PACKET_BUFFER_PADDING_SIZE, nxcip::MEDIA_DATA_BUFFER_ALIGNMENT );
}

unsigned int ISDAudioPacket::capacity() const
{
    return m_capacity;
}

void ISDAudioPacket::setDataSize( unsigned int _dataSize )
{
    m_dataSize = _dataSize;
}
