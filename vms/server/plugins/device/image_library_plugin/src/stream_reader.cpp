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
#include "ilp_empty_packet.h"
#include "motion_data_picture.h"

//if defined, random motion is generated
//#define GENERATE_RANDOM_MOTION
#ifdef GENERATE_RANDOM_MOTION
static const unsigned int MOTION_PRESENCE_CHANCE_PERCENT = 70;
#endif


static const nxcip::UsecUTCTimestamp USEC_IN_MS = 1000;
static const nxcip::UsecUTCTimestamp USEC_IN_SEC = 1000*1000;
static const nxcip::UsecUTCTimestamp NSEC_IN_USEC = 1000;

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    DirContentsManager* const dirContentsManager,
    unsigned int frameDurationUsec,
    bool liveMode,
    int encoderNumber )
:
    m_refManager( parentRefManager ),
    m_dirContentsManager( dirContentsManager ),
    m_curTimestamp( 0 ),
    m_frameDuration( frameDurationUsec ),
    m_liveMode( liveMode ),
    m_encoderNumber( encoderNumber ),
    m_streamReset( true ),
    m_nextFrameDeployTime( 0 ),
    m_isReverse( false ),
    m_cSeq( 0 )
{
    readDirContents();

    m_curPos = m_dirEntries.begin();
    m_curTimestamp = m_dirContentsManager->minTimestamp();
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
    unsigned int cSeq = 0;

    {
        Mutex::ScopedLock lk( &m_mutex );

        //copying data for consistency
        curTimestamp = m_curTimestamp;
        streamReset = m_streamReset;
        if( m_streamReset )
            m_streamReset = false;
        fileName = m_curPos == m_dirEntries.end() ? std::string() : m_curPos->second;
        isReverse = m_isReverse;
        cSeq = m_cSeq;
    }

    if( fileName.empty() )
    {
        //end of data
        *lpPacket = new ILPEmptyPacket(
            0,
            curTimestamp,
            (isReverse ? (nxcip::MediaDataPacket::fReverseBlockStart | nxcip::MediaDataPacket::fReverseStream) : 0),
            cSeq );
        return nxcip::NX_NO_ERROR;
    }

    std::unique_ptr<ILPVideoPacket> videoPacket( new ILPVideoPacket(
        0,
        curTimestamp,
        nxcip::MediaDataPacket::fKeyPacket |
            (streamReset ? nxcip::MediaDataPacket::fStreamReset : 0) |
            (isReverse ? (nxcip::MediaDataPacket::fReverseBlockStart | nxcip::MediaDataPacket::fReverseStream) : 0),
        cSeq ) );

#ifdef GENERATE_RANDOM_MOTION
    {
        if( rand() < RAND_MAX / 100.0 * MOTION_PRESENCE_CHANCE_PERCENT )
        {
            //generating random motion
            MotionDataPicture* motionData = new MotionDataPicture();

            const int motionRectX = rand() % (motionData->width() - 1);
            const int motionRectY = rand() % (motionData->height() - 1);
            const int motionRectWidth = rand() % (motionData->width() - motionRectX);
            const int motionRectHeight = rand() % (motionData->height() - motionRectY);
            motionData->fillRect( motionRectX, motionRectY, motionRectWidth, motionRectHeight, 1 );

            videoPacket->setMotionData( motionData );
            motionData->releaseRef();   //videoPacket takes reference to motionData
        }
    }
#endif

    struct stat fStat;
    memset( &fStat, 0, sizeof(fStat) );
    if( ::stat( fileName.c_str(), &fStat ) != 0 )
    {
        Mutex::ScopedLock lk( &m_mutex );
        moveCursorToNextFrame();
        return nxcip::NX_IO_ERROR;
    }

    videoPacket->resizeBuffer( fStat.st_size );
    if( !videoPacket->data() )
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
        bytesRead < videoPacket->dataSize() && !f.eof();
         )
    {
        f.read( (char*)videoPacket->data() + bytesRead, videoPacket->dataSize() - bytesRead );
        bytesRead += f.gcount();
    }
    f.close();

    {
        Mutex::ScopedLock lk( &m_mutex );
        moveCursorToNextFrame();
    }

    if( m_liveMode )
        doLiveDelay();  //emulating LIVE data delay

    *lpPacket = videoPacket.release();
    return nxcip::NX_NO_ERROR;
}

void StreamReader::interrupt()
{
    //TODO/IMPL
}

nxcip::UsecUTCTimestamp StreamReader::setPosition(
    unsigned int cSeq,
    nxcip::UsecUTCTimestamp timestamp )
{
    Mutex::ScopedLock lk( &m_mutex );

    if( m_dirEntries.empty() )
        return nxcip::INVALID_TIMESTAMP_VALUE;

    std::map<nxcip::UsecUTCTimestamp, std::string>::const_iterator it = m_dirEntries.begin();
    if( !m_isReverse )
    {
        //forward direction
        it = m_dirEntries.lower_bound( timestamp );
    }
    else
    {
        it = m_dirEntries.upper_bound( timestamp );
        if( it != m_dirEntries.begin() )
            --it;   //taking last frame
        else
            it = m_dirEntries.end();
    }

    m_curPos = it;
    m_curTimestamp = it == m_dirEntries.end() ? nxcip::INVALID_TIMESTAMP_VALUE : it->first;
    m_streamReset = true;
    m_cSeq = cSeq;

    return m_curTimestamp;
}

nxcip::UsecUTCTimestamp StreamReader::setReverseMode(
    unsigned int cSeq,
    bool isReverse,
    nxcip::UsecUTCTimestamp timestamp )
{
    Mutex::ScopedLock lk( &m_mutex );

    if( m_isReverse == isReverse && timestamp == nxcip::INVALID_TIMESTAMP_VALUE )
        return m_curTimestamp;

    m_isReverse = isReverse;
    if( timestamp == nxcip::INVALID_TIMESTAMP_VALUE )
    {
        m_cSeq = cSeq;
        return m_curTimestamp;
    }
    else
    {
        return setPosition( cSeq, timestamp );
    }
}

bool StreamReader::isReverse() const
{
    return m_isReverse;
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
    m_dirEntries = m_dirContentsManager->dirContents();
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
