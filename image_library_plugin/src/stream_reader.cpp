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
#include <sys/stat.h>

#include <fstream>
#include <memory>

#include "dir_contents_manager.h"
#include "dir_iterator.h"
#include "ilp_video_packet.h"


static const nxcip::UsecUTCTimestamp USEC_IN_MS = 1000;
static const nxcip::UsecUTCTimestamp USEC_IN_SEC = 1000*1000;
static const nxcip::UsecUTCTimestamp NSEC_IN_USEC = 1000;

StreamReader::StreamReader(
    CommonRefManager* const parentRefManager,
    const DirContentsManager& dirContentsManager,
    unsigned int frameDurationUsec,
    bool liveMode )
:
    m_refManager( parentRefManager ),
    m_dirContentsManager( dirContentsManager ),
    m_encoderNumber( 0 ),
    m_curTimestamp( 0 ),
    m_frameDuration( frameDurationUsec ),
    m_liveMode( liveMode ),
    m_streamReset( true ),
    m_nextFrameDeployTime( 0 ),
    m_isReverse( false )
{
    readDirContents();

    m_curPos = m_dirEntries.begin();
    m_curTimestamp = m_dirContentsManager.minTimestamp();
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
    nxcip::UsecUTCTimestamp curTimestamp = nxcip::INVALID_TIMESTAMP_VALUE;
    bool streamReset = false;
    std::string fileName;
    bool isReverse = false;

    {
        Mutex::ScopedLock lk( &m_mutex );

        if( m_curPos == m_dirEntries.end() )
        {
            *lpPacket = NULL;
            return nxcip::NX_NO_ERROR;    //end-of-data
        }

        //copying data for consistency
        curTimestamp = m_curTimestamp;
        streamReset = m_streamReset;
        if( m_streamReset )
            m_streamReset = false;
        fileName = m_curPos->second;
        isReverse = m_isReverse;
    }

    std::auto_ptr<ILPVideoPacket> packet( new ILPVideoPacket(
        m_encoderNumber,
        curTimestamp,
        nxcip::MediaDataPacket::fKeyPacket |
            (streamReset ? nxcip::MediaDataPacket::fStreamReset : 0) |
            (isReverse ? (nxcip::MediaDataPacket::fReverseBlockStart | nxcip::MediaDataPacket::fReverseStream) : 0) ) );

    struct stat fStat;
    memset( &fStat, 0, sizeof(fStat) );
    if( ::stat( fileName.c_str(), &fStat ) != 0 )
    {
        Mutex::ScopedLock lk( &m_mutex );
        moveCursorToNextFrame();
        return nxcip::NX_IO_ERROR;
    }

    packet->resizeBuffer( fStat.st_size );
    if( !packet->data() )
    {
        Mutex::ScopedLock lk( &m_mutex );
        moveCursorToNextFrame();
        return nxcip::NX_OTHER_ERROR;
    }

    //reading file into 
    std::ifstream f( fileName.c_str(), std::ios_base::in | std::ios_base::binary );
    if( !f.is_open() )
        return nxcip::NX_OTHER_ERROR;
    for( size_t
        bytesRead = 0;
        bytesRead < packet->dataSize() && !f.eof();
         )
    {
        f.read( (char*)packet->data() + bytesRead, packet->dataSize() - bytesRead );
        bytesRead += f.gcount();
    }
    f.close();

    {
        Mutex::ScopedLock lk( &m_mutex );
        moveCursorToNextFrame();
    }

    if( m_liveMode )
        doLiveDelay();  //emulating LIVE data delay

    *lpPacket = packet.release();
    return nxcip::NX_NO_ERROR;
}

void StreamReader::interrupt()
{
    //TODO/IMPL
}

nxcip::UsecUTCTimestamp StreamReader::setPosition( nxcip::UsecUTCTimestamp timestamp )
{
    Mutex::ScopedLock lk( &m_mutex );

    if( m_dirEntries.empty() )
        return nxcip::INVALID_TIMESTAMP_VALUE;

    std::map<nxcip::UsecUTCTimestamp, std::string>::const_iterator it = m_dirEntries.begin();
    if( m_isReverse )
    {
        it = m_dirEntries.lower_bound( timestamp );
        if( it == m_dirEntries.end() )
            --it;   //taking last frame
    }
    else
    {
        it = m_dirEntries.upper_bound( timestamp );
        if( it != m_dirEntries.begin() )
            --it;   //taking frame before requested
    }

    m_curPos = it;
    m_curTimestamp = it->first;
    m_streamReset = true;

    return m_curTimestamp;
}

nxcip::UsecUTCTimestamp StreamReader::setReverseMode(
    bool isReverse,
    nxcip::UsecUTCTimestamp timestamp )
{
    Mutex::ScopedLock lk( &m_mutex );

    if( m_isReverse == isReverse && timestamp == nxcip::INVALID_TIMESTAMP_VALUE )
        return m_curTimestamp;

    m_isReverse = isReverse;
    return timestamp == nxcip::INVALID_TIMESTAMP_VALUE ? m_curTimestamp : setPosition( timestamp );
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

void StreamReader::readDirContents()
{
    m_dirEntries = m_dirContentsManager.dirContents();
}

void StreamReader::moveCursorToNextFrame()
{
    if( m_isReverse )
    {
        if( m_curPos == m_dirEntries.begin() )
        {
            m_curPos = m_dirEntries.end();
            return;
        }
        else
        {
            --m_curPos;
        }
    }
    else
    {
        ++m_curPos;
    }

    if( m_liveMode && m_curPos == m_dirEntries.end() )
    {
        readDirContents();
        m_curPos = m_dirEntries.begin();
    }

    if( m_liveMode )
        m_curTimestamp += m_frameDuration;
    else
        m_curTimestamp = m_curPos != m_dirEntries.end() ? m_curPos->first : nxcip::INVALID_TIMESTAMP_VALUE;
}
