////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_SERVER_H
#define HLS_SERVER_H

#include <QDateTime>
#include <QMutex>
#include <QWaitCondition>

#include <core/resource/media_resource.h>
#include <utils/network/http/httpstreamreader.h>
#include <utils/network/tcp_connection_processor.h>

#include "../streaming_chunk.h"
#include "../streaming_chunk_cache_key.h"


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
        const QStringRef& uniqueResourceID,
        const std::multimap<QString, QString>& requestParams,
        nx_http::Response* const response );
    nx_http::StatusCode::Value getLivePlaylist(
        QnMediaResourcePtr mediaResource,
        const std::multimap<QString, QString>& requestParams,
        nx_http::Response* const response );
    nx_http::StatusCode::Value getArchivePlaylist(
        const QnMediaResourcePtr& mediaResource,
        const QDateTime& startTimestamp,
        const std::multimap<QString, QString>& requestParams,
        nx_http::Response* const response );
    nx_http::StatusCode::Value getResourceChunk(
        const nx_http::Request& request,
        const QStringRef& uniqueResourceID,
        const std::multimap<QString, QString>& requestParams,
        nx_http::Response* const response );

private slots:
    void chunkDataAvailable( StreamingChunk* pThis, quint64 newSizeBytes );
};

#endif  //HLS_SERVER_H
