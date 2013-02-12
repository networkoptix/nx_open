////////////////////////////////////////////////////////////
// 14 jan 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "mediaindex.h"

#include <QMutexLocker>


static const int MICROS_IN_MS = 1000;

MediaIndex::ChunkData::ChunkData()
:
    startTimestamp( 0 ),
    duration( 0 ),
    mediaSequence( 0 )
{
}

MediaIndex::ChunkData::ChunkData(
    quint64 _startTimestamp,
    quint64 _duration,
    unsigned int _mediaSequence )
:
    startTimestamp( _startTimestamp ),
    duration( _duration ),
    mediaSequence( _mediaSequence )
{
}

MediaIndex::MediaIndex()
{
}

void MediaIndex::addPoint(
    quint64 currentPacketTimestamp,
    const QDateTime& packetDateTime,
    bool isKeyFrame )
{
    if( !isKeyFrame )
        return;

    QMutexLocker lk( &m_mutex );

    m_dateTimeIndex[packetDateTime] = currentPacketTimestamp;
    m_timestampIndex.insert(currentPacketTimestamp);
}

unsigned int MediaIndex::generateChunkList(
    quint64 desiredStartTime,
    unsigned int targetDurationMSec,
    unsigned int chunksToGenerate,
    std::vector<ChunkData>* const chunkList ) const
{
    QMutexLocker lk( &m_mutex );

    if( m_timestampIndex.size() <= 1 )
        return 0;

    return generateChunkListNonSafe(
        getClosestChunkStartTimestamp( desiredStartTime, targetDurationMSec ),
        targetDurationMSec,
        chunksToGenerate,
        chunkList,
        true );
}

unsigned int MediaIndex::generateChunkListForLivePlayback(
    unsigned int targetDurationMSec,
    unsigned int chunksToGenerate,
    std::vector<ChunkData>* const chunkList ) const
{
    QMutexLocker lk( &m_mutex );

    if( m_timestampIndex.size() <= 1 )
        return 0;

    return generateChunkListNonSafe(
        getClosestChunkStartTimestamp( *(--m_timestampIndex.end()) - chunksToGenerate*targetDurationMSec*MICROS_IN_MS, targetDurationMSec ),
        targetDurationMSec,
        chunksToGenerate,
        chunkList,
        false );
}

quint64 MediaIndex::getClosestChunkStartTimestamp( quint64 desiredStartTime, unsigned int targetDurationMSec ) const
{
    if( m_timestampIndex.empty() )
        return 0;
    if( m_timestampIndex.size() == 1 )
        return *m_timestampIndex.begin();

    const quint64 alignedTimstamp = desiredStartTime / 1000 / targetDurationMSec;
    TimestampIndexType::const_iterator it = m_timestampIndex.upper_bound( alignedTimstamp );
    if( it != m_timestampIndex.begin() )
        --it;
    return *it;
}

unsigned int MediaIndex::generateChunkListNonSafe(
    const quint64 desiredStartTime,
    const unsigned int targetDurationMSec,
    const unsigned int chunksToGenerate,
    std::vector<ChunkData>* const chunkList,
    const bool allowSmallerLastChunk ) const
{
    Q_ASSERT( targetDurationMSec > 0 );

    chunkList->reserve( chunkList->size() + chunksToGenerate );

    //assuming that desiredStartTime is already aligned with getClosestChunkStartTimestamp
    Q_ASSERT( m_timestampIndex.find(desiredStartTime) != m_timestampIndex.end() );
    unsigned int chunksGenerated = 0;
    for( quint64
        currentChunkStart = desiredStartTime;
        chunksGenerated < chunksToGenerate;
        ++chunksGenerated )
    {
        //searching for farest key frame not further than \a targetDurationMSec
        TimestampIndexType::const_iterator nextChunkIter = m_timestampIndex.lower_bound( currentChunkStart+targetDurationMSec );
        if( nextChunkIter == m_timestampIndex.end() )
        {
            if( !allowSmallerLastChunk )
                break;
            else
                nextChunkIter = (++m_timestampIndex.rbegin()).base();
        }

        if( *nextChunkIter <= currentChunkStart )   //< is an actually assert condition
            break;

        chunkList->push_back( ChunkData(
            currentChunkStart,
            *nextChunkIter - currentChunkStart,
            currentChunkStart / (targetDurationMSec*MICROS_IN_MS) + 1 ) );
        currentChunkStart = *nextChunkIter;
    }

    return chunksGenerated > 0;
}
