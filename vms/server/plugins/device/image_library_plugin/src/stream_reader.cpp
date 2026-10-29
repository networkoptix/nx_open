// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_reader.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <thread>

#include "dir_contents_manager.h"
#include "dir_iterator.h"
#include "ilp_empty_packet.h"
#include "ilp_video_packet.h"
#include "motion_data_picture.h"

//if defined, random motion is generated
//#define GENERATE_RANDOM_MOTION
#ifdef GENERATE_RANDOM_MOTION
static const unsigned int MOTION_PRESENCE_CHANCE_PERCENT = 70;
#endif

StreamReader::StreamReader(nxpt::CommonRefManager* const parentRefManager,
    DirContentsManager* const dirContentsManager,
    unsigned int frameDurationUsec,
    bool liveMode,
    int encoderNumber):
    m_refManager(parentRefManager),
    m_dirContentsManager(dirContentsManager),
    m_curTimestamp(0),
    m_frameDuration(frameDurationUsec),
    m_liveMode(liveMode),
    m_encoderNumber(encoderNumber),
    m_streamReset(true),
    m_isReverse(false),
    m_cSeq(0)
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
int StreamReader::addRef() const
{
    return m_refManager.addRef();
}

//!Implementation of nxpl::PluginInterface::releaseRef
int StreamReader::releaseRef() const
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
    unsigned int cSeq, nxcip::UsecUTCTimestamp timestamp)
{
    Mutex::ScopedLock lk(&m_mutex);
    return setPositionUnsafe(cSeq, timestamp);
}

nxcip::UsecUTCTimestamp StreamReader::setPositionUnsafe(
    unsigned int cSeq, nxcip::UsecUTCTimestamp timestamp)
{
    if (m_dirEntries.empty())
        return nxcip::INVALID_TIMESTAMP_VALUE;

    m_curPos = std::invoke(
        [this, timestamp]
        {
            if (!m_isReverse)
                return m_dirEntries.lower_bound(timestamp);

            // The last frame at or before the timestamp.
            const auto next = m_dirEntries.upper_bound(timestamp);
            return next == m_dirEntries.begin() ? m_dirEntries.end() : std::prev(next);
        });
    m_curTimestamp =
        m_curPos == m_dirEntries.end() ? nxcip::INVALID_TIMESTAMP_VALUE : m_curPos->first;
    m_streamReset = true;
    m_cSeq = cSeq;

    return m_curTimestamp;
}

nxcip::UsecUTCTimestamp StreamReader::setReverseMode(
    unsigned int cSeq, bool isReverse, nxcip::UsecUTCTimestamp timestamp)
{
    Mutex::ScopedLock lk(&m_mutex);

    if (m_isReverse == isReverse && timestamp == nxcip::INVALID_TIMESTAMP_VALUE)
        return m_curTimestamp;

    m_isReverse = isReverse;
    if (timestamp != nxcip::INVALID_TIMESTAMP_VALUE)
        return setPositionUnsafe(cSeq, timestamp);

    m_cSeq = cSeq;
    return m_curTimestamp;
}

bool StreamReader::isReverse() const
{
    return m_isReverse;
}

void StreamReader::doLiveDelay()
{
    std::this_thread::sleep_until(m_nextFrameDeployTime);
    m_nextFrameDeployTime =
        std::chrono::steady_clock::now() + std::chrono::microseconds(m_frameDuration);
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
