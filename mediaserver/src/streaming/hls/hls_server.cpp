////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_server.h"

#include <algorithm>
#include <limits>
#include <map>

#include <core/resource_managment/resource_pool.h>
#include <utils/common/log.h>
#include <utils/media/mediaindex.h>
#include <utils/media/media_stream_cache.h>
#include <version.h>

#include "hls_types.h"
#include "../streaming_chunk_cache.h"
#include "../streaming_params.h"
#include "../../camera/camera_pool.h"


using namespace nx_http;
using namespace std;

static const size_t READ_BUFFER_SIZE = 64*1024;
static const double DEFAULT_TARGET_DURATION = 10;
static const int MIN_CHUNK_COUNT_IN_PLAYLIST = 3;
static const QLatin1String HLS_PREFIX( "/hls/" );
static const QLatin1String HLS_SESSION_PARAM_NAME( "hls_session" );
static const int MS_IN_SEC = 1000;

QnHttpLiveStreamingProcessor::QnHttpLiveStreamingProcessor( TCPSocket* socket, QnTcpListener* owner )
:
    QnTCPConnectionProcessor( socket, owner ),
    m_state( sReceiving ),
    m_currentChunk( NULL ),
    m_switchToChunkedTransfer( false ),
    m_useChunkedTransfer( false )
{
    m_readBuffer.reserve( READ_BUFFER_SIZE );
}

QnHttpLiveStreamingProcessor::~QnHttpLiveStreamingProcessor()
{
    if( m_currentChunk )
    {
        disconnect( m_currentChunk, SIGNAL(newDataIsAvailable(StreamingChunk*, quint64)), this, SLOT(chunkDataAvailable(StreamingChunk*, quint64)) );
        StreamingChunkCache::instance()->putBackUsedItem( m_currentChunkKey, m_currentChunk );
    }
}

void QnHttpLiveStreamingProcessor::run()
{
    while( !needToStop() )
    {
        switch( m_state )
        {
            case sReceiving:
                if( !receiveRequest() )
                    m_state = sDone;
                break;

            case sSending:
            {
                Q_ASSERT( !m_writeBuffer.isEmpty() );

                int bytesSent = 0;
                if( m_useChunkedTransfer )
                    bytesSent = sendChunk( m_writeBuffer ) ? m_writeBuffer.size() : -1;
                else
                    bytesSent = sendData( m_writeBuffer );
                if( bytesSent < 0 )
                {
                    NX_LOG( QString::fromLatin1("Error sending data to %1 (%2). Terminating connection...").
                        arg(remoteHostAddress()).arg(SystemError::getLastOSErrorText()), cl_logWARNING );
                    m_state = sDone;
                    break;
                }
                m_writeBuffer.remove( 0, bytesSent );
                if( !m_writeBuffer.isEmpty() )
                    break;  //continuing sending
                if( !prepareDataToSend() )
                {
                    NX_LOG( QString::fromLatin1("Finished uploading %1 data to %2. Closing connection...").
                        arg(QLatin1String("entity_path")).arg(remoteHostAddress()), cl_logDEBUG1 );
                    //sending empty chunk to signal EOF
                    if( m_useChunkedTransfer )
                        sendChunk( QByteArray() );
                    m_state = sDone;
                    break;
                }
                break;
            }

            case sDone:
                NX_LOG( QString::fromLatin1("Done request to %1. Closing connection...").arg(remoteHostAddress()), cl_logDEBUG1 );
                return;
        }
    }
}

bool QnHttpLiveStreamingProcessor::receiveRequest()
{
    if( m_readBuffer.isEmpty() )
    {
        m_readBuffer.resize( READ_BUFFER_SIZE );
        int bytesRead = readSocket( (quint8*)m_readBuffer.data(), m_readBuffer.size() );
        if( bytesRead < 0 )
        {
            NX_LOG( QString::fromLatin1("Error reading socket from %1 (%2). Terminating connection...").
                arg(remoteHostAddress()).arg(SystemError::getLastOSErrorText()), cl_logWARNING );
            return false;
        }
        if( bytesRead == 0 )
        {
            NX_LOG( QString::fromLatin1("Client %1 closed connection").arg(remoteHostAddress()), cl_logINFO );
            return false; 
        }
        m_readBuffer.resize( bytesRead );
    }

    size_t bytesProcessed = 0;
    if( !m_httpStreamReader.parseBytes( m_readBuffer, m_readBuffer.size(), &bytesProcessed ) )
    {
        NX_LOG( QString::fromLatin1("Error parsing http message from %1 (%2). Terminating connection...").
            arg(remoteHostAddress()).arg(m_httpStreamReader.errorText()), cl_logWARNING );
        return false;
    }
    if( bytesProcessed == m_readBuffer.size() )
        m_readBuffer.clear();    //small optimization. may be excessive
    else
        m_readBuffer.remove( 0, bytesProcessed );

    switch( m_httpStreamReader.state() )
    {
        case HttpStreamReader::readingMessageHeaders:
            //continuing reading
            return true;

        case HttpStreamReader::messageDone:
        case HttpStreamReader::readingMessageBody:
        case HttpStreamReader::waitingMessageStart:
            if( m_httpStreamReader.message().type != MessageType::request )
            {
                NX_LOG( QString::fromLatin1("Received %1 from %2 while expecting request. Terminating connection...").
                    arg(MessageType::toString(m_httpStreamReader.message().type)).arg(remoteHostAddress()), cl_logWARNING );
                return false;
            }
            m_state = sProcessingMessage;
            m_useChunkedTransfer = false;
            processRequest( *m_httpStreamReader.message().request );
            return true;

        case HttpStreamReader::parseError:
            //bad request format, terminating connection...
            NX_LOG( QString::fromLatin1("Error parsing http message from %1 (%2). Terminating connection...").
                arg(remoteHostAddress()).arg(m_httpStreamReader.errorText()), cl_logWARNING );
            return false;

        default:
            return false;
    }
}

void QnHttpLiveStreamingProcessor::processRequest( const nx_http::Request& request )
{
    nx_http::Response response;
    response.statusLine.version = request.requestLine.version;
    response.headers.insert( std::make_pair( "Date", QLocale(QLocale::English).toString(QDateTime::currentDateTime(), "ddd, d MMM yyyy hh:mm:ss GMT").toLatin1() ) );     //TODO/IMPL get timezone
    response.headers.insert( std::make_pair( "Server", QN_APPLICATION_NAME" "QN_APPLICATION_VERSION ) );
    response.headers.insert( std::make_pair( "Cache-Control", "no-cache" ) );   //findRequestedFile can override this

    //HttpFile file;
    response.statusLine.statusCode = getRequestedFile( request, &response/*, &file*/ );
    if( response.statusLine.reasonPhrase.isEmpty() )
        response.statusLine.reasonPhrase = StatusCode::toString( response.statusLine.statusCode );

    if( request.requestLine.version == nx_http::Version::http_1_1 )
    {
        if( response.headers.find("Transfer-Encoding") == response.headers.end() )
            response.headers.insert( std::make_pair( 
                "Transfer-Encoding",
                response.headers.find("Content-Length") != response.headers.end() ? "identity" : "chunked") );
        response.headers.insert( std::make_pair( "Connection", "close" ) ); //no persistent connections support
    }

    sendResponse( response );
}

nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getRequestedFile(
    const nx_http::Request& request,
    nx_http::Response* const response )
{
    //retreiving requested file name
    const QString& path = request.requestLine.url.path();
    int fileNameStartIndex = path.lastIndexOf(QChar('/'));
    if( fileNameStartIndex == -1 )
        return StatusCode::notFound;
    QStringRef fileName;
    if( fileNameStartIndex == path.size()-1 )   //path ends with /. E.g., /hls/camera1.m3u/. Skipping trailing /
    {
#if QT_VERSION >= 0x040800
        int newFileNameStartIndex = path.midRef(0, path.size()-1).lastIndexOf(QChar('/'));
        if( newFileNameStartIndex == -1 )
            return StatusCode::notFound;
#else
        //TODO delete this block after move to Qt4.8
        size_t newFileNameStartIndex = nx_http::find_last_of( path, QChar('/'), 0, path.size()-1 );
        if( newFileNameStartIndex == nx_http::BufferNpos )
            return StatusCode::notFound;
#endif
        fileName = path.midRef( newFileNameStartIndex+1, fileNameStartIndex-(newFileNameStartIndex+1) );
    }
    else
    {
        fileName = path.midRef( fileNameStartIndex+1 );
    }

    //parsing request parameters
    const QList<QPair<QString, QString> >& queryItemsList = request.requestLine.url.queryItems();
    std::multimap<QString, QString> requestParams;
    //moving params to map for more convenient use
    for( QList<QPair<QString, QString> >::const_iterator
        it = queryItemsList.begin();
        it != queryItemsList.end();
        ++it )
    {
        requestParams.insert( std::make_pair( it->first, it->second ) );
    }

    const QChar* extensionSepPos = std::find( fileName.constData(), fileName.constData()+fileName.size(), QChar('.') );
    if( extensionSepPos == fileName.constData()+fileName.size() )
    {
        //no extension, assuming stream has been requested...
        return getResourceChunk(
            request,
            fileName,
            requestParams,
            response );
    }
    else
    {
        //found extension
#if QT_VERSION >= 0x040800
        const QStringRef& extension = fileName.mid( extensionSepPos-fileName.constData()+1 );
        const QStringRef& shortFileName = fileName.mid( 0, extensionSepPos-fileName.constData() );
#else
        const QStringRef& extension = fileName.string()->midRef(
            fileName.position()+(extensionSepPos-fileName.constData()+1),
            fileName.size()-(extensionSepPos-fileName.constData()+1) );
        const QStringRef& shortFileName = fileName.string()->midRef(
            fileName.position(),
            extensionSepPos-fileName.constData() );
#endif
        if( extension.compare(QLatin1String("m3u")) == 0 || extension.compare(QLatin1String("m3u8")) == 0 )
            return getHLSPlaylist( shortFileName, requestParams, response );
    }

    return StatusCode::notFound;
}

void QnHttpLiveStreamingProcessor::sendResponse( const nx_http::Response& response )
{
    //serializing response to internal buffer
    nx_http::HttpHeaders::const_iterator it = response.headers.find( "Transfer-Encoding" );
    if( it != response.headers.end() && it->second == "chunked" )
        m_switchToChunkedTransfer = true;
    m_writeBuffer.clear();
    response.serialize( &m_writeBuffer );

    m_state = sSending;
}

bool QnHttpLiveStreamingProcessor::prepareDataToSend()
{
    Q_ASSERT( m_writeBuffer.isEmpty() );

    if( !m_currentChunk )
        return false;

    if( m_switchToChunkedTransfer )
    {
        m_useChunkedTransfer = true;
        m_switchToChunkedTransfer = false;
    }

    QMutexLocker lk( &m_mutex );
    for( ;; )
    {
        //reading chunk data
        if( m_currentChunk->tryRead( &m_chunkReadCtx, &m_writeBuffer ) )
            return !m_writeBuffer.isEmpty();

        //waiting for data to arrive to chunk
        m_cond.wait( lk.mutex() );
    }
}

nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getHLSPlaylist(
    const QStringRef& uniqueResourceID,
    const std::multimap<QString, QString>& requestParams,
    nx_http::Response* const response )
{
    //searching for requested resource
    QnResourcePtr resource = QnResourcePool::instance()->getResourceByUniqId( uniqueResourceID.toString() );
    if( !resource )
    {
        NX_LOG( QString::fromAscii("QnHttpLiveStreamingProcessor::getHLSPlaylist. Requested resource %1 not found").
            arg(QString::fromRawData(uniqueResourceID.data(), uniqueResourceID.size())), cl_logDEBUG1 );
        return nx_http::StatusCode::notFound;
    }

    QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>();
    if( !mediaResource )
    {
        NX_LOG( QString::fromAscii("QnHttpLiveStreamingProcessor::getHLSPlaylist. Requested resource %1 is not media resource").
            arg(QString::fromRawData(uniqueResourceID.data(), uniqueResourceID.size())), cl_logDEBUG1 );
        return nx_http::StatusCode::notFound;
    }

    std::multimap<QString, QString>::const_iterator startDatetimeIter = requestParams.find(QLatin1String(StreamingParams::START_DATETIME_PARAM_NAME));
    if( startDatetimeIter == requestParams.end() )
        return getLivePlaylist(
            mediaResource,
            requestParams,
            response );
    else
        return getArchivePlaylist(
            mediaResource,
            QDateTime::fromString(startDatetimeIter->second, Qt::ISODate),
            requestParams,
            response );

    //response->headers.insert( make_pair("Content-Type", "audio/mpegurl") );
    //response->messageBody = "<HTML><body>Hello, world</body></HTML>";
    //response->headers.insert( make_pair("Content-Length", QByteArray::number(response->messageBody.size()) ) );

    //return nx_http::StatusCode::ok;
}

typedef std::multimap<QString, QString> RequestParamsType;

nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getLivePlaylist(
    QnMediaResourcePtr mediaResource,
    const std::multimap<QString, QString>& requestParams,
    nx_http::Response* const response )
{
    QnVideoCamera* camera = qnCameraPool->getVideoCamera( mediaResource );
    if( !camera )
    {
        NX_LOG( QString::fromLatin1("Error. Requested live hls playlist of resource %1 which is not camera").arg(mediaResource->getUniqueId()), cl_logWARNING );
        return nx_http::StatusCode::forbidden;
    }

    //starting live caching, if it is not started
    if( !camera->ensureLiveCacheStarted() )
    {
        NX_LOG( QString::fromLatin1("Error. Requested live hls playlist of resource %1 with no live cache").arg(mediaResource->getUniqueId()), cl_logWARNING );
        return nx_http::StatusCode::internalServerError;
    }

    Q_ASSERT( camera->liveCache() );

    std::vector<MediaIndex::ChunkData> chunkList;
    const MediaIndex& mediaIndex = *camera->mediaIndex();
    if( !mediaIndex.generateChunkListForLivePlayback(
            DEFAULT_TARGET_DURATION * MS_IN_SEC,
            MIN_CHUNK_COUNT_IN_PLAYLIST,
            &chunkList )
        || chunkList.empty() )
    {
        NX_LOG( QString::fromLatin1("Failed to get live chunks of resource %1").arg(mediaResource->getUniqueId()), cl_logWARNING );
        return nx_http::StatusCode::noContent;
    }

    nx_hls::Playlist playlist;
    if( !chunkList.empty() )
        playlist.mediaSequence = chunkList[0].mediaSequence;
    playlist.closed = false;

    //TODO/IMPL special processing for empty playlist
    //if( playlist.chunks.empty() )
    //{
    //    //no media data cached yet
    //}

    //taking parameters, common for every chunks in playlist being generated
    RequestParamsType commonChunkParams;
    foreach( RequestParamsType::value_type param, requestParams )
    {
        if( param.first == StreamingParams::CHANNEL_PARAM_NAME ||
            param.first == StreamingParams::PICTURE_SIZE_PIXELS_PARAM_NAME ||
            param.first == StreamingParams::CONTAINER_FORMAT_PARAM_NAME ||
            param.first == StreamingParams::VIDEO_CODEC_PARAM_NAME ||
            param.first == StreamingParams::AUDIO_CODEC_PARAM_NAME ||
            param.first == HLS_SESSION_PARAM_NAME)
        {
            commonChunkParams.insert( param );
        }
    }

    for( std::vector<MediaIndex::ChunkData>::size_type
        i = 0;
        i < chunkList.size();
        ++i )
    {
        nx_hls::Chunk hlsChunk;
        hlsChunk.duration = chunkList[i].duration / 1000000.0;
        hlsChunk.url.setPath( HLS_PREFIX + mediaResource->getUniqueId() );
        foreach( RequestParamsType::value_type param, commonChunkParams )
            hlsChunk.url.addQueryItem( param.first, param.second );
        hlsChunk.url.addQueryItem( StreamingParams::START_TIMESTAMP_PARAM_NAME, QString::number(chunkList[i].startTimestamp) );
        hlsChunk.url.addQueryItem( StreamingParams::DURATION_MS_PARAM_NAME, QString::number(chunkList[i].duration) );
        playlist.chunks.push_back( hlsChunk );
    }

    response->messageBody = playlist.toString();

    return nx_http::StatusCode::ok;
}

nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getArchivePlaylist(
    const QnMediaResourcePtr& mediaResource,
    const QDateTime& startDatetime,
    const std::multimap<QString, QString>& requestParams,
    nx_http::Response* const response )
{
    //TODO/IMPL
    return nx_http::StatusCode::notImplemented;
}

nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getResourceChunk(
    const nx_http::Request& request,
    const QStringRef& uniqueResourceID,
    const std::multimap<QString, QString>& requestParams,
    nx_http::Response* const response )
{
    //reading parameters, generating cache key
    std::multimap<QString, QString>::const_iterator channelIter = requestParams.find(QLatin1String(StreamingParams::CHANNEL_PARAM_NAME));
    std::multimap<QString, QString>::const_iterator containerIter = requestParams.find(QLatin1String(StreamingParams::CONTAINER_FORMAT_PARAM_NAME));
    if( containerIter == requestParams.end() )
    {
        NX_LOG( QString::fromLatin1("Missing required parameter \"container\" in request from %1 for resource %2 stream").
            arg(remoteHostAddress()).arg(uniqueResourceID.toString()), cl_logDEBUG1 );
        return nx_http::StatusCode::badRequest;
    }
    std::multimap<QString, QString>::const_iterator startTimestampIter = requestParams.find(QLatin1String(StreamingParams::START_TIMESTAMP_PARAM_NAME));
    //std::multimap<QString, QString>::const_iterator endTimestampIter = requestParams.find(QLatin1String(StreamingParams::STOP_TIMESTAMP_PARAM_NAME));
    std::multimap<QString, QString>::const_iterator durationIter = requestParams.find(QLatin1String(StreamingParams::DURATION_MS_PARAM_NAME));
    //std::multimap<QString, QString>::const_iterator pictureSizeIter = requestParams.find(QLatin1String(StreamingParams::PICTURE_SIZE_PIXELS_PARAM_NAME));
    //std::multimap<QString, QString>::const_iterator audioCodecIter = requestParams.find(QLatin1String(StreamingParams::AUDIO_CODEC_PARAM_NAME));
    //std::multimap<QString, QString>::const_iterator videoCodecIter = requestParams.find(QLatin1String(StreamingParams::VIDEO_CODEC_PARAM_NAME));

    quint64 startTimestamp = 0;
    if( startTimestampIter != requestParams.end() )
    {
        startTimestamp = startTimestampIter->second.toULongLong();
    }
    else
    {
        std::multimap<QString, QString>::const_iterator startDatetimeIter = requestParams.find(QLatin1String(StreamingParams::START_DATETIME_PARAM_NAME));
        if( startDatetimeIter != requestParams.end() )
        {
            QDateTime startDatetime = QDateTime::fromString(startDatetimeIter->second, Qt::ISODate);
            //TODO/IMPL converting startDatetime to startTimestamp
        }
    }

    m_currentChunkKey = StreamingChunkCacheKey(
        uniqueResourceID.toString(),
        channelIter != requestParams.end()
            ? channelIter->second.toInt()
            : 0,   //any channel
        containerIter->second,
        startTimestamp,
        durationIter != requestParams.end()
            ? durationIter->second.toLongLong()
            : std::numeric_limits<quint64>::max(),
        //endTimestampIter != requestParams.end()
        //    ? QDateTime::fromString(endTimestampIter->second, Qt::ISODate)
        //    : QDateTime::fromTime_t(0xffffffff),
        requestParams );

    //retrieving streaming chunk
    StreamingChunk* chunk = StreamingChunkCache::instance()->takeForUse( m_currentChunkKey );
    if( !chunk )
    {
        NX_LOG( QString::fromLatin1("Could not get chunk %1 of resource %2 requested by %3").
            arg(QLatin1String(request.requestLine.url.encodedQuery())).arg(uniqueResourceID.toString()).arg(remoteHostAddress()), cl_logDEBUG1 );
        return nx_http::StatusCode::notFound;
    }

    //streaming chunk
    if( m_currentChunk )
    {
        disconnect( m_currentChunk, SIGNAL(newDataIsAvailable(StreamingChunk*, quint64)), this, SLOT(chunkDataAvailable(StreamingChunk*, quint64)) );
        StreamingChunkCache::instance()->putBackUsedItem( m_currentChunkKey, m_currentChunk );
    }

    m_currentChunk = chunk;
    connect(
        m_currentChunk, SIGNAL(newDataIsAvailable(StreamingChunk*, quint64)),
        this,           SLOT(chunkDataAvailable(StreamingChunk*, quint64)),
        Qt::DirectConnection );
    m_chunkReadCtx = StreamingChunk::SequentialReadingContext();

    response->headers.insert( make_pair( "Content-Type", m_currentChunk->mimeType().toLatin1() ) );
    response->headers.insert( make_pair( "Transfer-Encoding", "chunked" ) );
    response->statusLine.version = nx_http::Version::http_1_1;

    return nx_http::StatusCode::ok;
}

void QnHttpLiveStreamingProcessor::chunkDataAvailable( StreamingChunk* /*pThis*/, quint64 /*newSizeBytes*/ )
{
    QMutexLocker lk( &m_mutex );
    m_cond.wakeAll();
}
