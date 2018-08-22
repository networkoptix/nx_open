#include "ilp_media_packet.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <functional>

#include <plugins/plugin_tools.h>
#include <utils/memory/cyclic_allocator.h>

namespace nx {
namespace usb_cam {

ILPMediaPacket::ILPMediaPacket(
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

ILPMediaPacket::~ILPMediaPacket()
{
    if( m_buffer )
    {
        using namespace std::placeholders;
        nxpt::freeAligned( m_buffer, std::bind( &CyclicAllocator::release, m_allocator, _1 ) );
        m_buffer = NULL;
        m_bufSize = 0;
    }
}

void* ILPMediaPacket::queryInterface( const nxpl::NX_GUID& interfaceID )
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

unsigned int ILPMediaPacket::addRef()
{
    return m_refManager.addRef();
}

unsigned int ILPMediaPacket::releaseRef()
{
    return m_refManager.releaseRef();
}

nxcip::UsecUTCTimestamp ILPMediaPacket::timestamp() const
{
    return m_timestamp;
}

nxcip::DataPacketType ILPMediaPacket::type() const
{
    return m_mediaType;
}

const void* ILPMediaPacket::data() const
{
    return m_buffer;
}

unsigned int ILPMediaPacket::dataSize() const
{
    return m_bufSize;
}

unsigned int ILPMediaPacket::channelNumber() const
{
    return m_channelNumber;
}

nxcip::CompressionType ILPMediaPacket::codecType() const
{
    return m_codecType;
}

unsigned int ILPMediaPacket::flags() const
{
    return m_flags;
}

unsigned int ILPMediaPacket::cSeq() const
{
    return m_cSeq;
}

nxcip::Picture* ILPMediaPacket::getMotionData() const
{
    return NULL;
}

void ILPMediaPacket::setMediaType(nxcip::DataPacketType mediaType)
{
    m_mediaType = mediaType;
}

void ILPMediaPacket::setCodecType(nxcip::CompressionType codecType)
{
    m_codecType = codecType;
}

void ILPMediaPacket::resizeBuffer( size_t bufSize )
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

void* ILPMediaPacket::data()
{
    return m_buffer;
}

} // namespace usb_cam
} // namespace nx
