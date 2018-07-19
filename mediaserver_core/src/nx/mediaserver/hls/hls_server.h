#pragma once

#include <QDateTime>

#include <camera/video_camera.h>
#include <core/resource_access/user_access_data.h>
#include <core/resource/resource_fwd.h>
#include <network/tcp_connection_processor.h>
#include <nx/network/http/http_stream_reader.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <streaming/streaming_chunk.h>

#include "hls_playlist_manager.h"

struct QnJsonRestResult;

namespace nx {
namespace mediaserver {
namespace hls {

class Session;

struct RequestParams
{
    MediaQuality streamQuality = MEDIA_Quality_High;
    int channel = 0;
    QString containerFormat = "mpeg2ts";
    boost::optional<quint64> startTimestamp;
    boost::optional<std::chrono::microseconds> duration;
    boost::optional<QString> alias;
};

/*!
    HLS request url has following format:\n
    - /hls/unique-resource-id.m3u?format-parameters  - for playlist
    - /hls/unique-resource-id?format-parameters  - for chunks

    Parameters:\n

*/
class HttpLiveStreamingProcessor
:
    public QnTCPConnectionProcessor
{
    Q_OBJECT
    using base_type = QnTCPConnectionProcessor;
public:
    static bool doesPathEndWithCameraId() { return true; } //< See the base class method.

    HttpLiveStreamingProcessor(std::unique_ptr<nx::network::AbstractStreamSocket> socket, QnTcpListener* owner );
    virtual ~HttpLiveStreamingProcessor();

    /** Processes request, generates and sends response asynchronously. */
    void processRequest(const nx::network::http::Request& request);
    void prepareResponse(
        const nx::network::http::Request& request,
        nx::network::http::Response* const response);

    static void setMinPlayListSizeToStartStreaming(size_t value);

protected:
    virtual void run() override;
    const char* mimeTypeByExtension(const QString& extension) const;

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
    std::unique_ptr<StreamingChunkInputStream> m_chunkInputStream;
    QnMutex m_mutex;
    QnWaitCondition m_cond;
    bool m_switchToChunkedTransfer;
    bool m_useChunkedTransfer;
    QString m_currentFileName;
    size_t m_bytesSent;
    static size_t m_minPlaylistSizeToStartStreaming;

    //!
    /*!
        In case of success, adds Content-Type, Content-Length headers to \a response
    */
    nx::network::http::StatusCode::Value getRequestedFile(
        const nx::network::http::Request& request,
        nx::network::http::Response* const response,
        QnJsonRestResult* error);
    void sendResponse( const nx::network::http::Response& response );
    /*!
        \return false, if no more data to send (reached end of file)
    */
    bool prepareDataToSend();
    nx::network::http::StatusCode::Value getPlaylist(
        const nx::network::http::Request& request,
        const QString& requestFileExtension,
        const QnSecurityCamResourcePtr& camResource,
        const Qn::UserAccessData& accessRights,
        const QnVideoCameraPtr& videoCamera,
        const std::multimap<QString, QString>& requestParams,
        nx::network::http::Response* const response,
        QnJsonRestResult* error);
    //!Generates variant playlist (containing references to other playlists providing different qualities)
    nx::network::http::StatusCode::Value getVariantPlaylist(
        Session* session,
        const nx::network::http::Request& request,
        const QnSecurityCamResourcePtr& camResource,
        const QnVideoCameraPtr& videoCamera,
        const std::multimap<QString, QString>& requestParams,
        QByteArray* serializedPlaylist );
    //!Generates playlist with chunks inside
    nx::network::http::StatusCode::Value getChunkedPlaylist(
        Session* const session,
        const nx::network::http::Request& request,
        const QnSecurityCamResourcePtr& camResource,
        const std::multimap<QString, QString>& requestParams,
        QByteArray* serializedPlaylist );
    nx::network::http::StatusCode::Value getResourceChunk(
        const nx::network::http::Request& request,
        const QStringRef& uniqueResourceID,
        const QnSecurityCamResourcePtr& cameraResource,
        const std::multimap<QString, QString>& requestParams,
        nx::network::http::Response* const response );

    nx::network::http::StatusCode::Value createSession(
        const Qn::UserAccessData& accessRight,
        const QString& requestedPlaylistPath,
        const QString& sessionID,
        const std::multimap<QString, QString>& requestParams,
        const QnSecurityCamResourcePtr& camResource,
        const QnVideoCameraPtr& videoCamera,
        MediaQuality streamQuality,
        Session** session,
        QnJsonRestResult* error);
    int estimateStreamBitrate(
        Session* const session,
        QnSecurityCamResourcePtr camResource,
        const QnVideoCameraPtr& videoCamera,
        MediaQuality streamQuality );
    void ensureChunkCacheFilledEnoughForPlayback( Session* const session, MediaQuality streamQuality );

    AVCodecID detectAudioCodecId(const StreamingChunkCacheKey& chunkParams);

    static RequestParams readRequestParams(
        const std::multimap<QString, QString>& requestParams);
};
} // namespace hls
} // namespace mediaserver
} // namespace nx
