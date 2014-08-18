/**********************************************************
* 16 sep 2013
* akolesnikov
***********************************************************/

#include "ilp_empty_packet.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>


ILPEmptyPacket::ILPEmptyPacket(
    int channelNumber,
    nxcip::UsecUTCTimestamp _timestamp,
    unsigned int flags,
    unsigned int cSeq )
:
    m_refManager( this ),
    m_channelNumber( channelNumber ),
    m_timestamp( _timestamp ),
    m_flags( flags ),
    m_cSeq( cSeq )
{
}

ILPEmptyPacket::~ILPEmptyPacket()
{
}

//!Implementation of nxpl::PluginInterface::queryInterface
void* ILPEmptyPacket::queryInterface( const nxpl::NX_GUID& interfaceID )
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

//!Implementation of nxpl::PluginInterface::addRef
unsigned int ILPEmptyPacket::addRef()
{
    return m_refManager.addRef();
}

//!Implementation of nxpl::PluginInterface::releaseRef
unsigned int ILPEmptyPacket::releaseRef()
{
    return m_refManager.releaseRef();
}

//!Implementation of nxpl::MediaDataPacket::isKeyFrame
nxcip::UsecUTCTimestamp ILPEmptyPacket::timestamp() const
{
    return m_timestamp;
}

//!Implementation of nxpl::MediaDataPacket::type
nxcip::DataPacketType ILPEmptyPacket::type() const
{
    return nxcip::dptEmpty;
}

//!Implementation of nxpl::MediaDataPacket::data
const void* ILPEmptyPacket::data() const
{
    return 0;
}

//!Implementation of nxpl::MediaDataPacket::dataSize
unsigned int ILPEmptyPacket::dataSize() const
{
    return 0;
}

//!Implementation of nxpl::MediaDataPacket::channelNumber
unsigned int ILPEmptyPacket::channelNumber() const
{
    return m_channelNumber;
}

//!Implementation of nxpl::MediaDataPacket::codecType
nxcip::CompressionType ILPEmptyPacket::codecType() const
{
    return nxcip::CODEC_ID_MJPEG;
}

unsigned int ILPEmptyPacket::flags() const
{
    return m_flags;
}

unsigned int ILPEmptyPacket::cSeq() const
{
    return m_cSeq;
}

//!Implementation of nxpl::VideoDataPacket::getMotionData
nxcip::Picture* ILPEmptyPacket::getMotionData() const
{
    return 0;
}
