/**********************************************************
* 05 sep 2013
* akolesnikov
***********************************************************/

#include "ilp_video_packet.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>


ILPVideoPacket::ILPVideoPacket( int channelNumber, nxcip::UsecUTCTimestamp _timestamp )
:
    m_refManager( this ),
    m_channelNumber( channelNumber ),
    m_timestamp( _timestamp ),
    m_buffer( NULL ),
    m_bufSize( 0 )
{
}

ILPVideoPacket::~ILPVideoPacket()
{
    if( m_buffer )
    {
        ::free( m_buffer );
        m_buffer = NULL;
        m_bufSize = 0;
    }
}

//!Implementation of nxpl::PluginInterface::queryInterface
void* ILPVideoPacket::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_VideoDataPacket, sizeof(nxcip::IID_VideoDataPacket) ) == 0 )
    {
        addRef();
        return this;
    }
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
unsigned int ILPVideoPacket::addRef()
{
    return m_refManager.addRef();
}

//!Implementaion of nxpl::PluginInterface::releaseRef
unsigned int ILPVideoPacket::releaseRef()
{
    return m_refManager.releaseRef();
}

//!Implementation of nxpl::MediaDataPacket::isKeyFrame
nxcip::UsecUTCTimestamp ILPVideoPacket::timestamp() const
{
    return m_timestamp;
}

//!Implementation of nxpl::MediaDataPacket::type
nxcip::DataPacketType ILPVideoPacket::type() const
{
    return nxcip::dptVideo;
}

//!Implementation of nxpl::MediaDataPacket::data
const void* ILPVideoPacket::data() const
{
    return m_buffer;
}

//!Implementation of nxpl::MediaDataPacket::dataSize
unsigned int ILPVideoPacket::dataSize() const
{
    return m_bufSize;
}

//!Implementation of nxpl::MediaDataPacket::channelNumber
unsigned int ILPVideoPacket::channelNumber() const
{
    return m_channelNumber;
}

//!Implementation of nxpl::MediaDataPacket::codecType
nxcip::CompressionType ILPVideoPacket::codecType() const
{
    return nxcip::CODEC_ID_MJPEG;
}

unsigned int ILPVideoPacket::flags() const
{
    //TODO/IMPL
    return 0;
}

//!Implementation of nxpl::VideoDataPacket::getMotionData
nxcip::MotionData* ILPVideoPacket::getMotionData() const
{
    return NULL;
}

void ILPVideoPacket::resizeBuffer( unsigned int bufSize )
{
    if( bufSize < m_bufSize && bufSize > 0 )
    {
        m_bufSize = bufSize;
        return;
    }

    void* newBuffer = ::malloc( bufSize );

    if( m_bufSize > 0 )
    {
        if( newBuffer )
            memcpy( newBuffer, m_buffer, std::min<>(m_bufSize, bufSize) );
        ::free( m_buffer );
        m_buffer = NULL;
        m_bufSize = 0;
    }

    m_buffer = newBuffer;
    if( m_buffer )
        m_bufSize = bufSize;
}

void* ILPVideoPacket::data()
{
    return m_buffer;
}
