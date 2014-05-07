////////////////////////////////////////////////////////////
// 14 jan 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef MEDIAINDEX_H
#define MEDIAINDEX_H

#include <map>
#include <set>
#include <vector>

#include <QDateTime>
#include <QMutex>


/*!
    Used for quick seek in media stream.
    \note Thread-safe
*/
class MediaIndex
{
public:
    class ChunkData
    {
    public:
        //!internal timestamp (micros). Not a calendar time
        quint64 startTimestamp;
        //!in micros
        quint64 duration;
        //!sequential number of this chunk
        unsigned int mediaSequence;

        ChunkData();
        ChunkData(
            quint64 _startTimestamp,
            quint64 _duration,
            unsigned int _mediaSequence );
    };

    MediaIndex();

    /*!
        \param currentPacketTimestamp In micros
        \param packetDateTime
    */
    void addPoint(
        quint64 currentPacketTimestampUSec,
        const QDateTime& packetDateTime,
        bool isKeyFrame );
    /*!
        Resulting chunk duration almost always differs from \a targetDurationMSec
        \param desiredStartTime Timestamp in micros
        \param targetDurationMSec Desired chunk duration (in millis)
        \return Number of chunks generated
        \note Does not clear \a chunkList, but appends data to it
        \note Resulting chunk may exceed \a targetDurationMSec by GOP duration
    */
    unsigned int generateChunkList(
        quint64 desiredStartTime,
        unsigned int targetDurationMSec,
        unsigned int chunksToGenerate,
        std::vector<ChunkData>* const chunkList ) const;
    //!Same as \a generateChunkList, but returns \a chunksToGenerate last chunks of available data
    unsigned int generateChunkListForLivePlayback(
        unsigned int targetDurationMSec,
        unsigned int chunksToGenerate,
        std::vector<ChunkData>* const chunkList ) const;

private:
    typedef std::set<quint64> TimestampIndexType;
    typedef std::map<QDateTime, quint64> DateTimeIndexType;

    TimestampIndexType m_timestampIndex;
    DateTimeIndexType m_dateTimeIndex;
    mutable QMutex m_mutex;

    //!Returns timestamp of closest key frame to the left of \a desiredStartTime, aligned with \a targetDurationMSec to enable chunk caching
    /*!
        E.g., if \a targetDurationMSec is 10 sec, than value of 20 (or closest from the left key frame timestamp) sec will be returned for \a desiredStartTime of 20-29
        \param desiredStartTime micros
        \param targetDurationMicros micros
    */
    quint64 getClosestChunkStartTimestamp(
        quint64 desiredStartTime,
        unsigned int targetDurationMicros ) const;
    unsigned int generateChunkListNonSafe(
        quint64 desiredStartTime,
        unsigned int targetDurationMicros,
        unsigned int chunksToGenerate,
        std::vector<ChunkData>* const chunkList,
        bool allowSmallerLastChunk ) const;
};

#endif  //MEDIAINDEX_H
