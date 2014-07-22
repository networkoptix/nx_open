////////////////////////////////////////////////////////////
// 11 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_ARCHIVE_PLAYLIST_MANAGER_H
#define HLS_ARCHIVE_PLAYLIST_MANAGER_H

#include "hls_playlist_manager.h"

#include <deque>

#include <QtCore/QElapsedTimer>
#include <QtCore/QMutex>

#include <core/resource/resource_fwd.h>
#include <plugins/resource/avi/thumbnails_archive_delegate.h>


namespace nx_hls
{
    //!Generates sliding chunk list for archive playback
    /*!
        From hls point view this class generates "LIVE" playlist
        \note It is necessary to make archive playlist sliding since archive can be very long and playlist would be quite large (~ 25Mb for a month archive)
    */
    class ArchivePlaylistManager
    :
        public AbstractPlaylistManager
    {
    public:
        ArchivePlaylistManager(
            const QnSecurityCamResourcePtr& camResource,
            qint64 startTimestamp,
            unsigned int maxChunkNumberInPlaylist,
            qint64 targetDurationUsec,
            MediaQuality streamQuality );
        virtual ~ArchivePlaylistManager();

        bool initialize();

        //!Implementation of AbstractPlaylistManager::generateChunkList
        virtual size_t generateChunkList(
            std::vector<AbstractPlaylistManager::ChunkData>* const chunkList,
            bool* const endOfStreamReached ) const override;
        //!Implementation of AbstractPlaylistManager::getMaxBitrate
        virtual int getMaxBitrate() const override;

    private:
        const QnSecurityCamResourcePtr m_camResource;
        qint64 m_startTimestamp;
        unsigned int m_maxChunkNumberInPlaylist;
        qint64 m_targetDurationUsec;
        MediaQuality m_streamQuality;
        mutable QMutex m_mutex;
        std::deque<AbstractPlaylistManager::ChunkData> m_chunks;
        qint64 m_totalPlaylistDuration;
        qint64 m_prevChunkEndTimestamp;
        bool m_eof;
        int m_chunkMediaSequence;
        QnThumbnailsArchiveDelegate* m_delegate;
        QElapsedTimer m_playlistUpdateTimer;
        qint64 m_timerCorrection;
        bool m_initialPlaylistCreated;
        qint64 m_prevGeneratedChunkDuration;
        bool m_discontinuityDetected;
        //!archive chunk, that holds last found position
        QnAbstractArchiveDelegate::ArchiveChunkInfo m_currentArchiveChunk;

        void generateChunksIfNeeded();
        bool addOneMoreChunk();
        qint64 endTimestamp() const;
    };

    //!Using std::shared_ptr for \a std::shared_ptr::unique()
    typedef std::shared_ptr<ArchivePlaylistManager> ArchivePlaylistManagerPtr;
}

#endif  //HLS_ARCHIVE_PLAYLIST_MANAGER_H
