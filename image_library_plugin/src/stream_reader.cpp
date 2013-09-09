/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#include "stream_reader.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

#include <sys/timeb.h>
#include <sys/types.h>

#include <fstream>
#include <memory>

#include "ilp_video_packet.h"


static const nxcip::UsecUTCTimestamp USEC_IN_MS = 1000;
static const nxcip::UsecUTCTimestamp USEC_IN_SEC = 1000*1000;
static const nxcip::UsecUTCTimestamp NSEC_IN_USEC = 1000;

StreamReader::StreamReader(
    CommonRefManager* const parentRefManager,
    const std::string& imageDirectoryPath,
    unsigned int frameDurationUsec,
    bool liveMode )
:
    m_refManager( parentRefManager ),
    m_imageDirectoryPath( imageDirectoryPath ),
    m_dirIterator( imageDirectoryPath ),
    m_encoderNumber( 0 ),
    m_curTimestamp( 0 ),
    m_frameDuration( frameDurationUsec ),
    m_liveMode( liveMode ),
    m_lastPicTimestamp( 0 ),
    m_streamReset( true ),
    m_nextFrameDeployTime( 0 )
{
    struct timeb curTime;
    memset( &curTime, 0, sizeof(curTime) );
    ftime( &curTime );
    m_lastPicTimestamp = curTime.time * USEC_IN_SEC + curTime.millitm * USEC_IN_MS;

    m_dirIterator.setRecursive( true );
    m_dirIterator.setEntryTypeFilter( FsEntryType::etRegularFile );
    m_dirIterator.setWildCardMask( "*.jpg" );

    //reading directory
    while( m_dirIterator.next() )
        m_dirEntries.push_back( DirEntry( m_dirIterator.entryFullPath(), m_dirIterator.entrySize() ) );

    m_curPos = m_dirEntries.begin();
    m_curTimestamp = minTimestamp();
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
    if( m_curPos == m_dirEntries.end() )
    {
        *lpPacket = NULL;
        return nxcip::NX_NO_ERROR;    //end-of-data
    }

    std::auto_ptr<ILPVideoPacket> packet( new ILPVideoPacket(
        m_encoderNumber,
        m_curTimestamp,
        nxcip::MediaDataPacket::fKeyPacket | (m_streamReset ? nxcip::MediaDataPacket::fStreamReset : 0)) );
    if( m_streamReset )
        m_streamReset = false;
    packet->resizeBuffer( m_curPos->size );
    if( !packet->data() )
    {
        ++m_curPos;
        return nxcip::NX_OTHER_ERROR;
    }

    //reading file into 
    std::ifstream f( m_curPos->path.c_str(), std::ios_base::in | std::ios_base::binary );
    if( !f.is_open() )
        return nxcip::NX_OTHER_ERROR;
    packet->resizeBuffer( m_curPos->size );
    for( size_t
        bytesRead = 0;
        bytesRead < packet->dataSize() && !f.eof();
         )
    {
        f.read( (char*)packet->data() + bytesRead, packet->dataSize() - bytesRead );
        bytesRead += f.gcount();
    }
    f.close();

    if( m_liveMode )
        doLiveDelay();  //emulating LIVE data delay

    m_curTimestamp += m_frameDuration;
    ++m_curPos;
    if( m_liveMode && m_curPos == m_dirEntries.end() )
        m_curPos = m_dirEntries.begin();

    *lpPacket = packet.release();
    return nxcip::NX_NO_ERROR;
}

void StreamReader::interrupt()
{
    //TODO/IMPL
}

nxcip::UsecUTCTimestamp StreamReader::minTimestamp() const
{
    if( m_dirEntries.empty() )
        return nxcip::INVALID_TIMESTAMP_VALUE;
    return m_lastPicTimestamp - (m_dirEntries.size() - 1) * m_frameDuration;
}

nxcip::UsecUTCTimestamp StreamReader::maxTimestamp() const
{
    if( m_dirEntries.empty() )
        return nxcip::INVALID_TIMESTAMP_VALUE;
    return m_lastPicTimestamp;
}

void StreamReader::doLiveDelay()
{
    struct timeb curBTime;
    memset( &curBTime, 0, sizeof(curBTime) );
    ftime( &curBTime );
    const nxcip::UsecUTCTimestamp curTimeUsec = curBTime.time * USEC_IN_SEC + curBTime.millitm * USEC_IN_MS;

    if( m_nextFrameDeployTime > curTimeUsec )
    {
#ifdef _WIN32
        ::Sleep( (m_nextFrameDeployTime - curTimeUsec) / USEC_IN_MS );
#elif _POSIX_C_SOURCE >= 199309L
        struct timespec delay;
        memset( &delay, 0, sizeof(delay) );
        delay.tv_sec = (m_nextFrameDeployTime - curTimeUsec) / USEC_IN_SEC;
        delay.tv_nsec = ((m_nextFrameDeployTime - curTimeUsec) % USEC_IN_SEC) * NSEC_IN_USEC;
#else
        ::sleep( (m_nextFrameDeployTime - curTimeUsec) / USEC_IN_SEC );
#endif
    }

    ftime( &curBTime );

    m_nextFrameDeployTime = (curBTime.time * USEC_IN_SEC + curBTime.millitm * USEC_IN_MS) + m_frameDuration;
}
