/**********************************************************
* 05 sep 2013
* akolesnikov
***********************************************************/

#include "ilp_video_packet.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>

#include <plugins/plugin_tools.h>
#include <utils/memory/cyclic_allocator.h>


ILPVideoPacket::ILPVideoPacket(
    CyclicAllocator* const allocator,
    int channelNumber,
    nxcip::UsecUTCTimestamp _timestamp,
    unsigned int flags,
    unsigned int cSeq,
    nxcip::CompressionType codec)
:
    m_refManager( this ),
    m_allocator( allocator ),
    m_channelNumber( channelNumber ),
    m_timestamp( _timestamp ),
    m_buffer( NULL ),
    m_bufSize( 0 ),
    m_flags( flags ),
    m_motionData( NULL ),
    m_cSeq( cSeq ),
    m_codec( codec )
{
}

ILPVideoPacket::~ILPVideoPacket()
{
    if( m_buffer )
    {
        using namespace std::placeholders;
        nxpt::freeAligned( m_buffer, std::bind( &CyclicAllocator::release, m_allocator, _1 ) );
        m_buffer = NULL;
        m_bufSize = 0;
    }

    if( m_motionData )
    {
        m_motionData->releaseRef();
        m_motionData = NULL;
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
    return m_codec;
}

unsigned int ILPVideoPacket::flags() const
{
    return m_flags;
}

unsigned int ILPVideoPacket::cSeq() const
{
    return m_cSeq;
}

//!Implementation of nxpl::VideoDataPacket::getMotionData
nxcip::Picture* ILPVideoPacket::getMotionData() const
{
    if( m_motionData )
        m_motionData->addRef();
    return m_motionData;
}

void ILPVideoPacket::resizeBuffer( size_t bufSize )
{
    if( bufSize < m_bufSize && bufSize > 0 )
    {
        m_bufSize = bufSize;
        return;
    }

    using namespace std::placeholders;
    void* newBuffer = nxpt::mallocAligned(
        bufSize + nxcip::MEDIA_PACKET_BUFFER_PADDING_SIZE,
        nxcip::MEDIA_DATA_BUFFER_ALIGNMENT,
        std::bind( &CyclicAllocator::alloc, m_allocator, _1 ) );

    if( m_bufSize > 0 )
    {
        if( newBuffer )
            memcpy( newBuffer, m_buffer, std::min<>(m_bufSize, bufSize) );
        nxpt::freeAligned( m_buffer, std::bind( &CyclicAllocator::release, m_allocator, _1 ) );
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

void ILPVideoPacket::setMotionData( nxcip::Picture* motionData )
{
    m_motionData = motionData;
    if( m_motionData )
        m_motionData->addRef();
}
