////////////////////////////////////////////////////////////
// 21 feb 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_LIVE_PLAYLIST_MANAGER_H
#define HLS_LIVE_PLAYLIST_MANAGER_H

#include <deque>
#include <queue>

#include <QMutex>

#include <utils/media/media_stream_cache.h>


class MediaIndex;
class MediaStreamCache;

class HLSLivePlaylistManager
:
    public AbstractMediaCacheEventReceiver
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

    HLSLivePlaylistManager(
        MediaStreamCache* const mediaStreamCache,
        MediaIndex* const mediaIndex );
    ~HLSLivePlaylistManager();

    //!Same as \a generateChunkList, but returns \a chunksToGenerate last chunks of available data
    /*!
        \return Number of chunks generated
    */
    unsigned int generateChunkListForLivePlayback( std::vector<ChunkData>* const chunkList ) const;

private:
    MediaStreamCache* const m_mediaStreamCache;
    MediaIndex* const m_mediaIndex;
    std::deque<ChunkData> m_chunks;
    quint64 m_targetDurationUSec;
    quint64 m_prevTimestamp;
    size_t m_mediaSequence;
    ChunkData m_currentChunk;
    mutable QMutex m_mutex;
    quint64 m_totalPlaylistDuration;
    //queue<pair<timestamp to block; playlist start timestamp, blocking lives to> >
    std::queue<std::pair<quint64, quint64> > m_timestampToBlock;
    int m_blockID;

    //!Implementation of AbstractMediaCacheEventReceiver::onKeyFrame
    virtual void onKeyFrame( quint64 currentPacketTimestampUSec ) override;
};

#endif  //HLS_LIVE_PLAYLIST_MANAGER_H
