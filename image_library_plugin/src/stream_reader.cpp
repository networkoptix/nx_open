/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#include "stream_reader.h"

#include <fstream>

#include "archive_reader.h"
#include "ilp_video_packet.h"


StreamReader::StreamReader(
    ArchiveReader* const archiveReader,
    const std::string& imageDirectoryPath,
    unsigned int frameDurationUsec )
:
    m_refManager( archiveReader->refManager() ),
    m_imageDirectoryPath( imageDirectoryPath ),
    m_dirIterator( imageDirectoryPath ),
    m_encoderNumber( 0 ),
    m_curTimestamp( 0 ),
    m_frameDuration( frameDurationUsec )
{
    m_dirIterator.setRecursive( true );
    m_dirIterator.setEntryTypeFilter( FsEntryType::etRegularFile );
    m_dirIterator.setWildCardMask( "*.jpg" );
}

StreamReader::~StreamReader()
{
}

//!Implementation of nxpl::PluginInterface::queryInterface
void* StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_StreamReader, sizeof(nxcip::IID_StreamReader) ) == 0 )
    {
        addRef();
        return this;
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

//!Implementation of nxpl::PluginInterface::addRef
unsigned int StreamReader::addRef()
{
    return m_refManager.addRef();
}

//!Implementation of nxpl::PluginInterface::releaseRef
unsigned int StreamReader::releaseRef()
{
    return m_refManager.releaseRef();
}

int StreamReader::getNextData( nxcip::MediaDataPacket** lpPacket )
{
    if( !m_dirIterator.next() )
    {
        *lpPacket = NULL;
        return nxcip::NX_NO_ERROR;    //end-of-data
    }

    std::auto_ptr<ILPVideoPacket> packet( new ILPVideoPacket(m_encoderNumber, m_curTimestamp) );
    packet->resizeBuffer( m_dirIterator.entrySize() );
    if( !packet->data() )
        return nxcip::NX_OTHER_ERROR;

    //reading file into 
    std::ifstream f( m_dirIterator.entryAbsolutePath().c_str() );
    if( !f.is_open() )
        return nxcip::NX_OTHER_ERROR;
    f.setf( std::ios_base::binary );
    f.read( (char*)packet->data(), packet->dataSize() );
    packet->resizeBuffer( f.gcount() );

    m_curTimestamp += m_frameDuration;

    *lpPacket = packet.release();
    return nxcip::NX_NO_ERROR;
}

void StreamReader::interrupt()
{
    //TODO/IMPL
}
