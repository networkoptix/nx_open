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

size_t MediaIndex::generateChunkList(
    quint64 desiredStartTime,
    unsigned int targetDurationMSec,
    unsigned int chunksToGenerate,
    std::vector<ChunkData>* const chunkList ) const
{
    QMutexLocker lk( &m_mutex );

    if( m_timestampIndex.size() <= 1 )
        return 0;

    return generateChunkListNonSafe(
        desiredStartTime,
        targetDurationMSec*MICROS_IN_MS,
        chunksToGenerate,
        chunkList,
        true );
}

size_t MediaIndex::generateChunkListForLivePlayback(
    unsigned int targetDurationMSec,
    unsigned int chunksToGenerate,
    std::vector<ChunkData>* const chunkList ) const
{
    QMutexLocker lk( &m_mutex );

    if( m_timestampIndex.size() <= 1 )
        return 0;

    const quint64 desiredStartTime = *(--m_timestampIndex.end()) - *m_timestampIndex.begin() > chunksToGenerate*targetDurationMSec*MICROS_IN_MS
        ? (*(--m_timestampIndex.end()) - chunksToGenerate*targetDurationMSec*MICROS_IN_MS)
        : *m_timestampIndex.begin();

    return generateChunkListNonSafe(
        desiredStartTime,
        targetDurationMSec*MICROS_IN_MS,
        chunksToGenerate,
        chunkList,
        false );
}

quint64 MediaIndex::getClosestChunkStartTimestamp( quint64 desiredStartTime, unsigned int targetDurationMicros ) const
{
    if( m_timestampIndex.empty() )
        return 0;
    if( m_timestampIndex.size() == 1 )
        return *m_timestampIndex.begin();

    const quint64 alignedTimestamp = desiredStartTime / targetDurationMicros * targetDurationMicros;
    TimestampIndexType::const_iterator it = m_timestampIndex.upper_bound( alignedTimestamp );
    if( it != m_timestampIndex.begin() )
        --it;
    return *it;
}

size_t MediaIndex::generateChunkListNonSafe(
    const quint64 desiredStartTime,
    const unsigned int targetDurationMicros,
    const unsigned int chunksToGenerate,
    std::vector<ChunkData>* const chunkList,
    const bool /*allowSmallerLastChunk*/ ) const
{
    //TODO/IMPL allowSmallerLastChunk

    Q_ASSERT( targetDurationMicros > 0 );

    if( m_timestampIndex.empty() )
        return 0;

    chunkList->reserve( chunkList->size() + chunksToGenerate );

    unsigned int chunksGenerated = 0;
    quint64 alignedBorder = desiredStartTime / targetDurationMicros * targetDurationMicros;
    // if desiredStartTime is 25 and targetDurationMicros 10, then alignedBorder == 20

    //searching for timestamp closest to currentChunkStart from the left
    TimestampIndexType::const_iterator curChunkIter = m_timestampIndex.upper_bound( alignedBorder );
    if( curChunkIter != m_timestampIndex.begin() )
        --curChunkIter;
    for( ;
        chunksGenerated < chunksToGenerate;
        ++chunksGenerated )
    {
        alignedBorder += targetDurationMicros;
        TimestampIndexType::const_iterator nextChunkStartIter = m_timestampIndex.upper_bound( alignedBorder );
        if( nextChunkStartIter != m_timestampIndex.begin() )
            --nextChunkStartIter;

        if( nextChunkStartIter == curChunkIter )
            break;

        chunkList->push_back( ChunkData(
            *curChunkIter,
            *nextChunkStartIter - *curChunkIter,
            *curChunkIter / targetDurationMicros + 1 ) );
        curChunkIter = nextChunkStartIter;
    }

    return chunksGenerated;
}
