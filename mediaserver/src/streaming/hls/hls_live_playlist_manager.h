////////////////////////////////////////////////////////////
// 21 feb 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_LIVE_PLAYLIST_MANAGER_H
#define HLS_LIVE_PLAYLIST_MANAGER_H

#include <deque>
#include <memory>
#include <queue>

#include <QtCore/QElapsedTimer>
#include <QtCore/QMutex>

#include "hls_playlist_manager.h"


class MediaStreamCache;

namespace nx_hls
{
    /*!
        - Provides live playlist for resource
        - Updates playlist in real-time (by registering itself as an MediaStreamCache event receiver)
        - Blocks must-be-available chunks data from removal from MediaStreamCache
    */
    class HLSLivePlaylistManager
    :
        public AbstractPlaylistManager
    {
    public:
        HLSLivePlaylistManager(
            MediaStreamCache* const mediaStreamCache,
            quint64 targetDurationUSec );
        virtual ~HLSLivePlaylistManager();

        //!Returns playlist for live data
        /*!
            \return Number of chunks generated
        */
        virtual size_t generateChunkList(
            std::vector<AbstractPlaylistManager::ChunkData>* const chunkList,
            bool* const endOfStreamReached ) const override;
        //!Implementation of AbstractPlaylistManager::getMaxBitrate
        virtual int getMaxBitrate() const override;

        void clear();
        //!Time (millis) from last usage of this object
        size_t inactivityPeriod() const;

    private:
        MediaStreamCache* const m_mediaStreamCache;
        mutable std::deque<AbstractPlaylistManager::ChunkData> m_chunks;
        const quint64 m_targetDurationUSec;
        mutable unsigned int m_mediaSequence;
        AbstractPlaylistManager::ChunkData m_currentChunk;
        mutable QMutex m_mutex;
        quint64 m_totalPlaylistDuration;
        //queue<pair<timestamp to block; playlist start timestamp, blocking lives to> >
        std::queue<std::pair<quint64, quint64> > m_timestampToBlock;
        int m_blockID;
        const int m_removedChunksToKeepCount;
        int m_eventRegistrationID;
        mutable QElapsedTimer m_inactivityTimer;

        void onKeyFrame( quint64 currentPacketTimestampUSec );
    };

    //!Using std::shared_ptr for \a std::shared_ptr::unique()
    typedef std::shared_ptr<nx_hls::HLSLivePlaylistManager> HLSLivePlaylistManagerPtr;
}

#endif  //HLS_LIVE_PLAYLIST_MANAGER_H
