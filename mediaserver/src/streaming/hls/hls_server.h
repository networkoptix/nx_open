////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_SERVER_H
#define HLS_SERVER_H

#include <QDateTime>
#include <QMutex>
#include <QWaitCondition>

#include <core/resource/media_resource.h>
#include <utils/media/mediaindex.h>
#include <utils/network/http/httpstreamreader.h>
#include <utils/network/tcp_connection_processor.h>

#include "hls_playlist_manager.h"
#include "../streaming_chunk.h"
#include "../streaming_chunk_cache_key.h"


namespace nx_hls
{
    class HLSSession;

    /*!
        HLS request url has following format:\n
        - /hls/unique-resource-id.m3u?format-parameters  - for playlist
        - /hls/unique-resource-id?format-parameters  - for chunks

        Parameters:\n

    */
    class QnHttpLiveStreamingProcessor
    :
        virtual public QnTCPConnectionProcessor
    {
        Q_OBJECT

    public:
        QnHttpLiveStreamingProcessor( TCPSocket* socket, QnTcpListener* owner );
        virtual ~QnHttpLiveStreamingProcessor();

    protected:
        virtual void run() override;

    private:
        enum State
        {
            sReceiving,
            sProcessingMessage,
            sSending,
            sDone
        };

        nx_http::HttpStreamReader m_httpStreamReader;
        State m_state;
        nx_http::BufferType m_readBuffer;
        nx_http::BufferType m_writeBuffer;
        StreamingChunk* m_currentChunk;
        StreamingChunk::SequentialReadingContext m_chunkReadCtx;
        QMutex m_mutex;
        QWaitCondition m_cond;
        bool m_switchToChunkedTransfer;
        bool m_useChunkedTransfer;
        StreamingChunkCacheKey m_currentChunkKey;

        /*!
            \return false in case if error
        */
        bool receiveRequest();
        //!Processes \a request, generates and sends (asynchronously) response
        void processRequest( const nx_http::Request& request );
        //!
        /*!
            In case of success, adds Content-Type, Content-Length headers to \a response
        */
        nx_http::StatusCode::Value getRequestedFile( const nx_http::Request& request, nx_http::Response* const response );
        void sendResponse( const nx_http::Response& response );
        /*!
            \return false, if no more data to send (reached end of file)
        */
        bool prepareDataToSend();
        nx_http::StatusCode::Value getHLSPlaylist(
            //HLSSession* const session,
            const nx_http::Request& request,
            const QStringRef& uniqueResourceID,
            const std::multimap<QString, QString>& requestParams,
            nx_http::Response* const response );
        //!Generates variant playlist (containing references to other playlists providing different qualities)
        nx_http::StatusCode::Value getHLSVariantPlaylist(
            HLSSession* session,
            const nx_http::Request& request,
            const QStringRef& uniqueResourceID,
            const std::multimap<QString, QString>& requestParams,
            nx_http::Response* const response );
        //!Generates playlist with chunks inside
        nx_http::StatusCode::Value getHLSChunkedPlaylist(
            HLSSession* const session,
            const nx_http::Request& request,
            const QStringRef& uniqueResourceID,
            const std::multimap<QString, QString>& requestParams,
            nx_http::Response* const response );
        nx_http::StatusCode::Value getLiveChunkPlaylist(
            HLSSession* const session,
            QnMediaResourcePtr mediaResource,
            const std::multimap<QString, QString>& requestParams,
            std::vector<nx_hls::AbstractPlaylistManager::ChunkData>* const chunkList );
        nx_http::StatusCode::Value getArchiveChunkPlaylist(
            HLSSession* const session,
            const QnMediaResourcePtr& mediaResource,
            const std::multimap<QString, QString>& requestParams,
            std::vector<nx_hls::AbstractPlaylistManager::ChunkData>* const chunkList,
            bool* const isPlaylistClosed );
        nx_http::StatusCode::Value getResourceChunk(
            const nx_http::Request& request,
            const QStringRef& uniqueResourceID,
            const std::multimap<QString, QString>& requestParams,
            nx_http::Response* const response );

    private slots:
        void chunkDataAvailable( StreamingChunk* pThis, quint64 newSizeBytes );
    };
}

#endif  //HLS_SERVER_H
