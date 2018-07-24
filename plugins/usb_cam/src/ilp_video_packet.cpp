#include "ilp_video_packet.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <functional>

#include <plugins/plugin_tools.h>
#include <utils/memory/cyclic_allocator.h>

namespace nx {
namespace usb_cam {

ILPVideoPacket::ILPVideoPacket(
    CyclicAllocator* const allocator,
    int channelNumber,
    nxcip::UsecUTCTimestamp _timestamp,
    unsigned int flags,
    unsigned int cSeq )
:
    m_refManager( this ),
    m_allocator( allocator ),
    m_channelNumber( channelNumber ),
    m_timestamp( _timestamp ),
    m_buffer( NULL ),
    m_bufSize( 0 ),
    m_flags( flags ),
    m_cSeq( cSeq )
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
}

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

unsigned int ILPVideoPacket::addRef()
{
    return m_refManager.addRef();
}

unsigned int ILPVideoPacket::releaseRef()
{
    return m_refManager.releaseRef();
}

nxcip::UsecUTCTimestamp ILPVideoPacket::timestamp() const
{
    return m_timestamp;
}

nxcip::DataPacketType ILPVideoPacket::type() const
{
    return nxcip::dptVideo;
}

const void* ILPVideoPacket::data() const
{
    return m_buffer;
}

unsigned int ILPVideoPacket::dataSize() const
{
    return m_bufSize;
}

unsigned int ILPVideoPacket::channelNumber() const
{
    return m_channelNumber;
}

nxcip::CompressionType ILPVideoPacket::codecType() const
{
    return m_codecType;
}

unsigned int ILPVideoPacket::flags() const
{
    return m_flags;
}

unsigned int ILPVideoPacket::cSeq() const
{
    return m_cSeq;
}

nxcip::Picture* ILPVideoPacket::getMotionData() const
{
    return NULL;
}

void ILPVideoPacket::setCodecType(nxcip::CompressionType codecType)
{
    m_codecType = codecType;
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

} // namespace usb_cam
} // namespace nx
