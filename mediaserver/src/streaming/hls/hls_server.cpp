////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_server.h"

#include <QtCore/QUrlQuery>

#include <algorithm>
#include <limits>
#include <map>

#include <core/resource_management/resource_pool.h>
#include <utils/common/log.h>
#include <utils/common/systemerror.h>
#include <utils/media/media_stream_cache.h>
#include <version.h>

#include "camera/camera_pool.h"
#include "hls_archive_playlist_manager.h"
#include "hls_live_playlist_manager.h"
#include "hls_session_pool.h"
#include "hls_types.h"
#include "streaming/streaming_chunk_cache.h"
#include "streaming/streaming_params.h"
#include "utils/network/tcp_connection_priv.h"


using namespace nx_http;
using namespace std;

extern QnLog* requestsLog;

namespace nx_hls
{
    static const size_t READ_BUFFER_SIZE = 64*1024;
    static const double DEFAULT_TARGET_DURATION = 10;
    static const int DEFAULT_HLS_SESSION_LIVE_TIMEOUT = (int)DEFAULT_TARGET_DURATION * 7;
    static const int MIN_CHUNK_COUNT_IN_PLAYLIST = 3;
    static const int CHUNK_COUNT_IN_ARCHIVE_PLAYLIST = 3;
    static const QLatin1String HLS_PREFIX( "/hls/" );
    static const QLatin1String HLS_SESSION_PARAM_NAME( "hls_session" );
    static const quint64 MS_IN_SEC = 1000;
    static const quint64 MICROS_IN_MS = 1000;

    QnHttpLiveStreamingProcessor::QnHttpLiveStreamingProcessor( QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner )
    :
        QnTCPConnectionProcessor( socket ),
        m_state( sReceiving ),
        m_currentChunk( NULL ),
        m_switchToChunkedTransfer( false ),
        m_useChunkedTransfer( false )
    {
        m_readBuffer.reserve( READ_BUFFER_SIZE );
        setObjectName( "QnHttpLiveStreamingProcessor" );
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

                case sProcessingMessage:
                    Q_ASSERT( false );
                    break;

                case sSending:
                {
                    Q_ASSERT( !m_writeBuffer.isEmpty() );

                    int bytesSent = 0;
                    if( m_useChunkedTransfer )
                        bytesSent = sendChunk( m_writeBuffer ) ? m_writeBuffer.size() : -1;
                    else
                        bytesSent = sendData( m_writeBuffer ) ? m_writeBuffer.size() : -1;;
                    if( bytesSent < 0 )
                    {
                        NX_LOG( QString::fromLatin1("Error sending data to %1 (%2). Terminating connection...").
                            arg(remoteHostAddress().toString()).arg(SystemError::getLastOSErrorText()), cl_logWARNING );
                        m_state = sDone;
                        break;
                    }
                    m_writeBuffer.remove( 0, bytesSent );
                    if( !m_writeBuffer.isEmpty() )
                        break;  //continuing sending
                    if( !prepareDataToSend() )
                    {
                        NX_LOG( QString::fromLatin1("Finished uploading %1 data to %2. Closing connection...").
                            arg(QLatin1String("entity_path")).arg(remoteHostAddress().toString()), cl_logDEBUG1 );
                        //sending empty chunk to signal EOF
                        if( m_useChunkedTransfer )
                            sendChunk( QByteArray() );
                        m_state = sDone;
                        break;
                    }
                    break;
                }

                case sDone:
                    NX_LOG( QString::fromLatin1("Done request to %1. Closing connection...").arg(remoteHostAddress().toString()), cl_logDEBUG1 );
                    return;
            }
        }
    }

    bool QnHttpLiveStreamingProcessor::receiveRequest()
    {
        Q_D(QnTCPConnectionProcessor);
        if( !d->clientRequest.isEmpty() )
            m_readBuffer = std::move(d->clientRequest);

        if( m_readBuffer.isEmpty() )
        {
            m_readBuffer.resize( READ_BUFFER_SIZE );
            int bytesRead = readSocket( (quint8*)m_readBuffer.data(), m_readBuffer.size() );
            if( bytesRead < 0 )
            {
                NX_LOG( QString::fromLatin1("Error reading socket from %1 (%2). Terminating connection...").
                    arg(remoteHostAddress().toString()).arg(SystemError::getLastOSErrorText()), cl_logWARNING );
                return false;
            }
            if( bytesRead == 0 )
            {
                NX_LOG( QString::fromLatin1("Client %1 closed connection").arg(remoteHostAddress().toString()), cl_logINFO );
                return false; 
            }
            m_readBuffer.resize( bytesRead );
        }

        size_t bytesProcessed = 0;
        if( !m_httpStreamReader.parseBytes( m_readBuffer, m_readBuffer.size(), &bytesProcessed ) )
        {
            NX_LOG( QString::fromLatin1("Error parsing http message from %1 (%2). Terminating connection...").
                arg(remoteHostAddress().toString()).arg(m_httpStreamReader.errorText()), cl_logWARNING );
            return false;
        }
        if( bytesProcessed == (size_t)m_readBuffer.size() )
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
                if( requestsLog )
                    requestsLog->log( QString::fromLatin1("Received %1 from %2:\n%3-------------------\n\n\n").
                        arg(nx_http::MessageType::toString(m_httpStreamReader.message().type)).
                        arg(remoteHostAddress().toString()).
                        arg(QString::fromLatin1(m_httpStreamReader.message().toString())), cl_logDEBUG1 );

                if( m_httpStreamReader.message().type != MessageType::request )
                {
                    NX_LOG( QString::fromLatin1("Received %1 from %2 while expecting request. Terminating connection...").
                        arg(MessageType::toString(m_httpStreamReader.message().type)).arg(remoteHostAddress().toString()), cl_logWARNING );
                    return false;
                }
                m_state = sProcessingMessage;
                m_useChunkedTransfer = false;
                processRequest( *m_httpStreamReader.message().request );
                return true;

            case HttpStreamReader::parseError:
                //bad request format, terminating connection...
                NX_LOG( QString::fromLatin1("Error parsing http message from %1 (%2). Terminating connection...").
                    arg(remoteHostAddress().toString()).arg(m_httpStreamReader.errorText()), cl_logWARNING );
                return false;

            default:
                return false;
        }
    }

    void QnHttpLiveStreamingProcessor::processRequest( const nx_http::Request& request )
    {
        nx_http::Response response;
        response.statusLine.version = request.requestLine.version;
        response.headers.insert( std::make_pair(
            "Date",
            QLocale(QLocale::English).toString(QDateTime::currentDateTime(), "ddd, d MMM yyyy hh:mm:ss GMT").toLatin1() ) );     //TODO/IMPL/HLS get timezone
        response.headers.insert( std::make_pair( "Server", QN_APPLICATION_NAME " " QN_APPLICATION_VERSION ) );
        response.headers.insert( std::make_pair( "Cache-Control", "no-cache" ) );   //findRequestedFile can override this

        response.statusLine.statusCode = getRequestedFile( request, &response );
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
            int newFileNameStartIndex = path.midRef(0, path.size()-1).lastIndexOf(QChar('/'));
            if( newFileNameStartIndex == -1 )
                return StatusCode::notFound;
            fileName = path.midRef( newFileNameStartIndex+1, fileNameStartIndex-(newFileNameStartIndex+1) );
        }
        else
        {
            fileName = path.midRef( fileNameStartIndex+1 );
        }

        //parsing request parameters
        const QList<QPair<QString, QString> >& queryItemsList = QUrlQuery(request.requestLine.url.query()).queryItems();
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
            //no extension, assuming media stream has been requested...
            return getResourceChunk(
                request,
                fileName,
                requestParams,
                response );
        }
        else
        {
            //found extension
            const QStringRef& extension = fileName.mid( extensionSepPos-fileName.constData()+1 );
            const QStringRef& shortFileName = fileName.mid( 0, extensionSepPos-fileName.constData() );
            if( extension.compare(QLatin1String("m3u")) == 0 || extension.compare(QLatin1String("m3u8")) == 0 )
                return getHLSPlaylist(
                    request,
                    shortFileName,
                    requestParams,
                    response );
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

        if( requestsLog )
            requestsLog->log( QString::fromLatin1("Sending response to %1:\n%2-------------------\n\n\n").
                arg(remoteHostAddress().toString()).
                arg(QString::fromLatin1(m_writeBuffer)), cl_logDEBUG1 );

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

    typedef std::multimap<QString, QString> RequestParamsType;

    nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getHLSPlaylist(
        const nx_http::Request& request,
        const QStringRef& uniqueResourceID,
        const std::multimap<QString, QString>& requestParams,
        nx_http::Response* const response )
    {
        //searching for session (if specified)
        std::multimap<QString, QString>::const_iterator sessionIDIter = requestParams.find( StreamingParams::SESSION_ID_PARAM_NAME );
        QString sessionID = sessionIDIter != requestParams.end()
            ? sessionIDIter->second
            : HLSSessionPool::generateUniqueID();

        //session search and add MUST be atomic
        HLSSessionPool::ScopedSessionIDLock lk( HLSSessionPool::instance(), sessionID );

        HLSSession* session = HLSSessionPool::instance()->find( sessionID );
        if( !session )
        {
            std::multimap<QString, QString>::const_iterator startDatetimeIter = requestParams.find(StreamingParams::START_DATETIME_PARAM_NAME);
            session = new HLSSession(
                sessionID,
                startDatetimeIter == requestParams.end() ); //if no start date specified, providing live stream
            HLSSessionPool::instance()->add( session, DEFAULT_HLS_SESSION_LIVE_TIMEOUT );
            if( startDatetimeIter != requestParams.end() )
            {
                //TODO/IMPL/HLS
                    //converting startDatetime to timestamp
                quint64 startTimestamp = 0;
                //generating sliding playlist, holding not more than CHUNK_COUNT_IN_ARCHIVE_PLAYLIST archive chunks
                session->setPlaylistManager( QSharedPointer<AbstractPlaylistManager>(
                    new ArchivePlaylistManager(
                        startTimestamp,
                        CHUNK_COUNT_IN_ARCHIVE_PLAYLIST)) );
            }
        }

        std::multimap<QString, QString>::const_iterator chunkedParamIter = requestParams.find( StreamingParams::CHUNKED_PARAM_NAME );
        if( chunkedParamIter == requestParams.end() )
            return getHLSVariantPlaylist( session, request, uniqueResourceID, requestParams, response );
        else
            return getHLSChunkedPlaylist( session, request, uniqueResourceID, requestParams, response );
    }

    nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getHLSVariantPlaylist(
        HLSSession* session,
        const nx_http::Request& request,
        const QStringRef& /*uniqueResourceID*/,
        const std::multimap<QString, QString>& /*requestParams*/,
        nx_http::Response* const response )
    {
        nx_hls::VariantPlaylist playlist;

        QUrl baseUrl;
        nx_http::HttpHeaders::const_iterator hostIter = request.headers.find( "Host" );
        if( hostIter != request.headers.end() )
        {
            SocketAddress sockAddress( hostIter->second );
            baseUrl.setHost( sockAddress.address.toString() );
            if( sockAddress.port > 0 )
                baseUrl.setPort( sockAddress.port );
            baseUrl.setScheme( QLatin1String("http") );
        }

        //TODO: #ak check resource stream type. Only h.264 is OK for HLS

        nx_hls::VariantPlaylistData playlistData;
        playlistData.url = baseUrl;
        playlistData.url.setPath( request.requestLine.url.path() );
        playlistData.bandwidth = 1280000;   //TODO/IMPL add real bitrate
        QList<QPair<QString, QString> > queryItems = QUrlQuery(request.requestLine.url.query()).queryItems();
        //removing SESSION_ID_PARAM_NAME
        for( QList<QPair<QString, QString> >::iterator
            it = queryItems.begin();
            it != queryItems.end();
             )
        {
            if( (it->first == StreamingParams::CHUNKED_PARAM_NAME) || (it->first == StreamingParams::SESSION_ID_PARAM_NAME) )
                queryItems.erase( it++ );
            else
                ++it;
        }
        QUrlQuery playlistDataQuery;
        playlistDataQuery.setQueryItems( queryItems );
        playlistDataQuery.addQueryItem( StreamingParams::CHUNKED_PARAM_NAME, QString() );
        playlistDataQuery.addQueryItem( StreamingParams::SESSION_ID_PARAM_NAME, session->id() );
        playlistData.url.setQuery(playlistDataQuery);
        playlist.playlists.push_back(playlistData);

        //TODO/IMPL/HLS adding low quality playlist url (if there is low quality stream for resource)

        response->messageBody = playlist.toString();
        response->headers.insert( make_pair("Content-Type", "audio/mpegurl") );
        response->headers.insert( make_pair("Content-Length", QByteArray::number(response->messageBody.size()) ) );

        return nx_http::StatusCode::ok;
    }

    nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getHLSChunkedPlaylist(
        HLSSession* const session,
        const nx_http::Request& request,
        const QStringRef& uniqueResourceID,
        const std::multimap<QString, QString>& requestParams,
        nx_http::Response* const response )
    {
        //searching for requested resource
        QnResourcePtr resource = QnResourcePool::instance()->getResourceByUniqId( uniqueResourceID.toString() );
        if( !resource )
        {
            NX_LOG( QString::fromLatin1("QnHttpLiveStreamingProcessor::getHLSPlaylist. Requested resource %1 not found").
                arg(QString::fromRawData(uniqueResourceID.data(), uniqueResourceID.size())), cl_logDEBUG1 );
            return nx_http::StatusCode::notFound;
        }

        QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>();
        if( !mediaResource )
        {
            NX_LOG( QString::fromLatin1("QnHttpLiveStreamingProcessor::getHLSPlaylist. Requested resource %1 is not media resource").
                arg(QString::fromRawData(uniqueResourceID.data(), uniqueResourceID.size())), cl_logDEBUG1 );
            return nx_http::StatusCode::notFound;
        }

        Q_ASSERT( session );

        std::vector<nx_hls::AbstractPlaylistManager::ChunkData> chunkList;
        bool isPlaylistClosed = false;
        const nx_http::StatusCode::Value playlistResult = session->isLive()
            ? getLiveChunkPlaylist(
                session,
                mediaResource,
                requestParams,
                &chunkList )
            : getArchiveChunkPlaylist(
                session,
                mediaResource,
                requestParams,
                &chunkList,
                &isPlaylistClosed );
        if( playlistResult != nx_http::StatusCode::ok )
        {
            NX_LOG( QString::fromLatin1("QnHttpLiveStreamingProcessor::getHLSPlaylist. Failed to compose playlist for resource %1 streaming").
                arg(QString::fromRawData(uniqueResourceID.data(), uniqueResourceID.size())), cl_logDEBUG1 );
            return playlistResult;
        }

        nx_hls::Playlist playlist;
        if( !chunkList.empty() )
            playlist.mediaSequence = chunkList[0].mediaSequence;
        playlist.closed = isPlaylistClosed;

        //TODO/IMPL/HLS special processing for empty playlist
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

        QUrl baseChunkUrl;
        nx_http::HttpHeaders::const_iterator hostIter = request.headers.find( "Host" );
        if( hostIter != request.headers.end() )
        {
            SocketAddress sockAddress( hostIter->second );
            baseChunkUrl.setHost( sockAddress.address.toString() );
            if( sockAddress.port > 0 )
                baseChunkUrl.setPort( sockAddress.port );
            baseChunkUrl.setScheme( QLatin1String("http") );
        }

        for( std::vector<nx_hls::AbstractPlaylistManager::ChunkData>::size_type
            i = 0;
            i < chunkList.size();
            ++i )
        {
            nx_hls::Chunk hlsChunk;
            hlsChunk.duration = chunkList[i].duration / 1000000.0;
            hlsChunk.url = baseChunkUrl;
            hlsChunk.url.setPath( HLS_PREFIX + mediaResource->toResource()->getUniqueId() );
            QUrlQuery hlsChunkUrlQuery( hlsChunk.url.query() );
            foreach( RequestParamsType::value_type param, commonChunkParams )
                hlsChunkUrlQuery.addQueryItem( param.first, param.second );
            hlsChunkUrlQuery.addQueryItem( StreamingParams::START_TIMESTAMP_PARAM_NAME, QString::number(chunkList[i].startTimestamp) );
            hlsChunkUrlQuery.addQueryItem( StreamingParams::DURATION_MS_PARAM_NAME, QString::number(chunkList[i].duration) );
            if( session->isLive() )
                hlsChunkUrlQuery.addQueryItem( StreamingParams::LIVE_PARAM_NAME, QString() );
            hlsChunk.url.setQuery( hlsChunkUrlQuery );
            playlist.chunks.push_back( hlsChunk );
        }

        response->messageBody = playlist.toString();

        response->headers.insert( make_pair("Content-Type", "audio/mpegurl") );
        response->headers.insert( make_pair("Content-Length", QByteArray::number(response->messageBody.size()) ) );

        return playlistResult;
    }

    nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getLiveChunkPlaylist(
        HLSSession* const /*session*/,
        QnMediaResourcePtr mediaResource,
        const std::multimap<QString, QString>& /*requestParams*/,
        std::vector<nx_hls::AbstractPlaylistManager::ChunkData>* const chunkList )
    {
        QnVideoCamera* camera = qnCameraPool->getVideoCamera( mediaResource->toResourcePtr() );
        if( !camera )
        {
            NX_LOG( QString::fromLatin1("Error. Requested live hls playlist of resource %1 which is not camera").arg(mediaResource->toResource()->getUniqueId()), cl_logWARNING );
            return nx_http::StatusCode::forbidden;
        }

        //starting live caching, if it is not started
        if( !camera->ensureLiveCacheStarted() )
        {
            NX_LOG( QString::fromLatin1("Error. Requested live hls playlist of resource %1 with no live cache").arg(mediaResource->toResource()->getUniqueId()), cl_logWARNING );
            return nx_http::StatusCode::internalServerError;
        }

        Q_ASSERT( camera->liveCache() );

        Q_ASSERT( camera->hlsLivePlaylistManager() );
        const unsigned int chunksGenerated = camera->hlsLivePlaylistManager()->generateChunkList( chunkList, NULL );
        if( chunksGenerated == 0 )
        {
            NX_LOG( QString::fromLatin1("Failed to get live chunks of resource %1").arg(mediaResource->toResource()->getUniqueId()), cl_logWARNING );
            return nx_http::StatusCode::noContent;
        }

        NX_LOG( QString::fromLatin1("Prepared live playlist of resource %1 (%2 chunks)").arg(mediaResource->toResource()->getUniqueId()).arg(chunksGenerated), cl_logDEBUG2 );

        return nx_http::StatusCode::ok;
    }

    //TODO/IMPL/HLS looks like it is possible to merge getLiveChunkPlaylist and getArchiveChunkPlaylist

    nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getArchiveChunkPlaylist(
        HLSSession* const session,
        const QnMediaResourcePtr& mediaResource,
        const std::multimap<QString, QString>& /*requestParams*/,
        std::vector<nx_hls::AbstractPlaylistManager::ChunkData>* const chunkList,
        bool* const isPlaylistClosed )
    {
        Q_ASSERT( session );

        QnVideoCamera* camera = qnCameraPool->getVideoCamera( mediaResource->toResourcePtr() );
        if( !camera )
        {
            NX_LOG( QString::fromLatin1("Error. Requested hls playlist of resource %1 which is not camera").arg(mediaResource->toResource()->getUniqueId()), cl_logWARNING );
            return nx_http::StatusCode::forbidden;
        }

        //timestamp here - is a duration from beginning of the archive

        const unsigned int chunksGenerated = session->playlistManager()->generateChunkList( chunkList, isPlaylistClosed );
        if( chunksGenerated == 0 )
        {
            NX_LOG( QString::fromLatin1("Failed to get archive chunks of resource %1").arg(mediaResource->toResource()->getUniqueId()), cl_logWARNING );
            return nx_http::StatusCode::noContent;
        }

        NX_LOG( QString::fromLatin1("Prepared archive playlist of resource %1 (%2 chunks)").arg(mediaResource->toResource()->getUniqueId()).arg(chunksGenerated), cl_logDEBUG2 );

        return nx_http::StatusCode::ok;
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
        QString containerFormat;
        if( containerIter != requestParams.end() )
            containerFormat = containerIter->second;
        else
            containerFormat = "mpegts";
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
                    //this is secondary functionality, not used by this HLS implementation (since all chunks are referenced by npt timestamps)
            }
        }

        m_currentChunkKey = StreamingChunkCacheKey(
            uniqueResourceID.toString(),
            channelIter != requestParams.end()
                ? channelIter->second.toInt()
                : 0,   //any channel
            containerFormat,
            startTimestamp,
            durationIter != requestParams.end()
                ? durationIter->second.toLongLong()
                : DEFAULT_TARGET_DURATION * MS_IN_SEC * MICROS_IN_MS,
                //: std::numeric_limits<quint64>::max(),  //TODO/IMPL support downloading to the end, but that chunk MUST not be cached!
            //endTimestampIter != requestParams.end()
            //    ? QDateTime::fromString(endTimestampIter->second, Qt::ISODate)
            //    : QDateTime::fromTime_t(0xffffffff),
            requestParams );

        //retrieving streaming chunk
        StreamingChunk* chunk = StreamingChunkCache::instance()->takeForUse( m_currentChunkKey );
        if( !chunk )
        {
            NX_LOG( QString::fromLatin1("Could not get chunk %1 of resource %2 requested by %3").
                arg(request.requestLine.url.query()).arg(uniqueResourceID.toString()).arg(remoteHostAddress().toString()), cl_logDEBUG1 );
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
        if( m_currentChunk->isClosed() && m_currentChunk->sizeInBytes() > 0 )   //TODO/IMPL is condition sutisfying?
	  response->headers.insert( make_pair( "Content-Length", nx_http::StringType::number((qlonglong)m_currentChunk->sizeInBytes()) ) );
        else
            response->headers.insert( make_pair( "Transfer-Encoding", "chunked" ) );
        response->statusLine.version = nx_http::Version::http_1_1;

        nx_http::HttpHeaders::const_iterator rangeIter = request.headers.find( "Range" );

        return rangeIter != request.headers.end() ? nx_http::StatusCode::partialContent : nx_http::StatusCode::ok;
    }

    void QnHttpLiveStreamingProcessor::chunkDataAvailable( StreamingChunk* /*pThis*/, quint64 /*newSizeBytes*/ )
    {
        QMutexLocker lk( &m_mutex );
        m_cond.wakeAll();
    }
}
