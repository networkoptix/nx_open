////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_SERVER_H
#define HLS_SERVER_H

#include <QDateTime>
#include <QMutex>
#include <QWaitCondition>

#include <core/resource/resource_fwd.h>
#include <utils/network/http/httpstreamreader.h>
#include <utils/network/tcp_connection_processor.h>

#include "camera/video_camera.h"
#include "hls_playlist_manager.h"
#include "../streaming_chunk.h"


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
        public QnTCPConnectionProcessor
    {
        Q_OBJECT

    public:
        QnHttpLiveStreamingProcessor( QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner );
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

        State m_state;
        nx::Buffer m_writeBuffer;
        StreamingChunkPtr m_currentChunk;
        //!For reading data from \a m_currentChunk
        std::unique_ptr<AbstractInputByteStream> m_chunkInputStream;
        QMutex m_mutex;
        QWaitCondition m_cond;
        bool m_switchToChunkedTransfer;
        bool m_useChunkedTransfer;
        QString m_currentFileName;
        size_t m_bytesSent;

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
        nx_http::StatusCode::Value getPlaylist(
            const nx_http::Request& request,
            const QnSecurityCamResourcePtr& camResource,
            const QnVideoCameraPtr& videoCamera,
            const std::multimap<QString, QString>& requestParams,
            nx_http::Response* const response );
        //!Generates variant playlist (containing references to other playlists providing different qualities)
        nx_http::StatusCode::Value getVariantPlaylist(
            HLSSession* session,
            const nx_http::Request& request,
            const QnSecurityCamResourcePtr& camResource,
            const QnVideoCameraPtr& videoCamera,
            const std::multimap<QString, QString>& requestParams,
            nx_http::Response* const response );
        //!Generates playlist with chunks inside
        nx_http::StatusCode::Value getChunkedPlaylist(
            HLSSession* const session,
            const nx_http::Request& request,
            const QnSecurityCamResourcePtr& camResource,
            const std::multimap<QString, QString>& requestParams,
            nx_http::Response* const response );
        nx_http::StatusCode::Value getResourceChunk(
            const nx_http::Request& request,
            const QStringRef& uniqueResourceID,
            const QnSecurityCamResourcePtr& camResource,
            const std::multimap<QString, QString>& requestParams,
            nx_http::Response* const response );

        nx_http::StatusCode::Value createSession(
            const QString& requestedPlaylistPath,
            const QString& sessionID,
            const std::multimap<QString, QString>& requestParams,
            const QnSecurityCamResourcePtr& camResource,
            const QnVideoCameraPtr& videoCamera,
            MediaQuality streamQuality,
            HLSSession** session );
        int estimateStreamBitrate(
            HLSSession* const session,
            QnSecurityCamResourcePtr camResource,
            const QnVideoCameraPtr& videoCamera,
            MediaQuality streamQuality );
        void ensureChunkCacheFilledEnoughForPlayback( HLSSession* const session, MediaQuality streamQuality );

    private slots:
        void chunkDataAvailable( StreamingChunkPtr chunk, quint64 newSizeBytes );
    };
}

#endif  //HLS_SERVER_H
