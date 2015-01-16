////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_server.h"

#include <QtCore/QUrlQuery>
#include <QtCore/QTimeZone>
#include <QtCore/QCoreApplication>

#include <algorithm>
#include <limits>
#include <map>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <utils/common/log.h>
#include <utils/common/systemerror.h>
#include <utils/media/ffmpeg_helper.h>
#include <utils/media/media_stream_cache.h>
#include <utils/common/app_info.h>

#include "camera/camera_pool.h"
#include "hls_archive_playlist_manager.h"
#include "hls_live_playlist_manager.h"
#include "hls_session_pool.h"
#include "hls_types.h"
#include "media_server/settings.h"
#include "streaming/streaming_chunk_cache.h"
#include "streaming/streaming_params.h"
#include "utils/network/tcp_connection_priv.h"


//TODO #ak if camera has hi stream only, than playlist request with no quality specified returns No Content, hi returns OK, lo returns Not Found

using std::make_pair;
using namespace nx_http;

namespace nx_hls
{
    static const size_t READ_BUFFER_SIZE = 64*1024;
    static const int MIN_CHUNK_COUNT_IN_PLAYLIST = 3;
    static const int CHUNK_COUNT_IN_ARCHIVE_PLAYLIST = 3;
    static const QLatin1String HLS_PREFIX( "/hls/" );
    static const quint64 MSEC_IN_SEC = 1000;
    static const quint64 USEC_IN_MSEC = 1000;
    static const quint64 USEC_IN_SEC = MSEC_IN_SEC * USEC_IN_MSEC;
    static const unsigned int DEFAULT_HLS_SESSION_LIVE_TIMEOUT_MS = nx_ms_conf::DEFAULT_TARGET_DURATION_MS * 7;
    static const int COMMON_KEY_FRAME_TO_NON_KEY_FRAME_RATIO = 5;
    static const int DEFAULT_PRIMARY_STREAM_BITRATE = 4*1024*1024;
    static const int DEFAULT_SECONDARY_STREAM_BITRATE = 512*1024;

    QnHttpLiveStreamingProcessor::QnHttpLiveStreamingProcessor( QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* /*owner*/ )
    :
        QnTCPConnectionProcessor( socket ),
        m_state( sReceiving ),
        m_switchToChunkedTransfer( false ),
        m_useChunkedTransfer( false ),
        m_bytesSent( 0 )
    {
        m_readBuffer.reserve( READ_BUFFER_SIZE );
        setObjectName( "QnHttpLiveStreamingProcessor" );
    }

    QnHttpLiveStreamingProcessor::~QnHttpLiveStreamingProcessor()
    {
        //while we are here only QnHttpLiveStreamingProcessor::chunkDataAvailable slot can be called

        if( m_currentChunk )
        {
            //disconnecting and waiting for already-emitted signals from m_currentChunk to be delivered and processed
            //TODO #ak cancel on-going transcoding. Currently, it just wastes CPU time
            m_currentChunk->disconnectAndJoin( this );
            StreamingChunkCache::instance()->putBackUsedItem( m_currentChunk->params(), m_currentChunk );
            m_chunkInputStream.reset();
            m_currentChunk.reset();
        }

        //TODO/HLS: #ak clean up archive chunk data, since we do not cache archive chunks
            //this should be done automatically by cache: should mark archive chunks as "uncachable"
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
                        bytesSent = sendData( m_writeBuffer ) ? m_writeBuffer.size() : -1;
                    if( bytesSent < 0 )
                    {
                        NX_LOG( QString::fromLatin1("Error sending data to %1 (%2). Sent %3 bytes total. Terminating connection...").
                            arg(remoteHostAddress().toString()).arg(SystemError::getLastOSErrorText()).arg(m_bytesSent), cl_logWARNING );
                        m_state = sDone;
                        break;
                    }
                    else
                    {
                        m_bytesSent += bytesSent;
                    }
                    if( bytesSent == m_writeBuffer.size() )
                        m_writeBuffer.clear();
                    else
                        m_writeBuffer.remove( 0, bytesSent );
                    if( !m_writeBuffer.isEmpty() )
                        break;  //continuing sending
                    if( !prepareDataToSend() )
                    {
                        NX_LOG( QString::fromLatin1("Finished uploading %1 data to %2. Sent %3 bytes total. Closing connection...").
                            arg(m_currentFileName).arg(remoteHostAddress().toString()).arg(m_bytesSent), cl_logDEBUG1 );
                        //sending empty chunk to signal EOF
                        if( m_useChunkedTransfer )
                            sendChunk( QByteArray() );
                        m_state = sDone;
                        break;
                    }
                    break;
                }

                case sDone:
                    NX_LOG( QnLog::HTTP_LOG_INDEX, QString::fromLatin1("Done message to %1. Closing connection...\n\n\n").arg(remoteHostAddress().toString()), cl_logDEBUG1 );
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

        response.statusLine.statusCode = getRequestedFile( request, &response );
        if( response.statusLine.reasonPhrase.isEmpty() )
            response.statusLine.reasonPhrase = StatusCode::toString( response.statusLine.statusCode );

        response.headers.insert( std::make_pair(
            "Date",
            QLocale(QLocale::English).toString(QDateTime::currentDateTime(), lit("ddd, d MMM yyyy hh:mm:ss t")).toLatin1() ) );
        response.headers.insert( std::make_pair( "Server", (qApp->applicationName() + lit(" ") + QCoreApplication::applicationVersion()).toLatin1().data()) );
        response.headers.insert( std::make_pair( "Cache-Control", "no-cache" ) );   //getRequestedFile can override this

        if( request.requestLine.version == nx_http::http_1_1 )
        {
            if( (response.statusLine.statusCode / 100 == 2) && (response.headers.find("Transfer-Encoding") == response.headers.end()) )
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
        m_currentFileName = fileName.toString();

        const int extensionSepPos = fileName.indexOf( QChar('.') );
        const QStringRef& extension = extensionSepPos != -1 ? fileName.mid( extensionSepPos+1 ) : QStringRef();
        const QStringRef& shortFileName = fileName.mid( 0, extensionSepPos );

        //searching for requested resource
        QnResourcePtr resource = QnResourcePool::instance()->getResourceByUniqId( shortFileName.toString() );
        if( !resource )
        {
            NX_LOG( QString::fromLatin1("QnHttpLiveStreamingProcessor::getPlaylist. Requested resource %1 not found").
                arg(QString::fromRawData(shortFileName.constData(), shortFileName.size())), cl_logDEBUG1 );
            return nx_http::StatusCode::notFound;
        }

        QnSecurityCamResourcePtr camResource = resource.dynamicCast<QnSecurityCamResource>();
        if( !camResource )
        {
            NX_LOG( QString::fromLatin1("QnHttpLiveStreamingProcessor::getPlaylist. Requested resource %1 is not camera").
                arg(QString::fromRawData(shortFileName.constData(), shortFileName.size())), cl_logDEBUG1 );
            return nx_http::StatusCode::notFound;
        }

        //checking resource stream type. Only h.264 is OK for HLS
        QnVideoCamera* camera = qnCameraPool->getVideoCamera( camResource );
        if( !camera )
        {
            NX_LOG( QString::fromLatin1("Error. HLS request to resource %1 which is not camera").arg(camResource->getUniqueId()), cl_logDEBUG2 );
            return nx_http::StatusCode::forbidden;
        }

        QnConstCompressedVideoDataPtr lastVideoFrame = camera->getLastVideoFrame( true, 0 );
        if( lastVideoFrame && (lastVideoFrame->compressionType != CODEC_ID_H264) && (lastVideoFrame->compressionType != CODEC_ID_NONE) )
        {
            //video is not in h.264 format
            NX_LOG( QString::fromLatin1("Error. HLS request to resource %1 with codec %2").
                arg(camResource->getUniqueId()).arg(codecIDToString(lastVideoFrame->compressionType)), cl_logWARNING );
            return nx_http::StatusCode::forbidden;
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

        if( extension.isEmpty() || extension == QLatin1String("ts") )
        {
            //no extension, assuming media stream has been requested...
            return getResourceChunk(
                request,
                fileName,
                camResource,
                requestParams,
                response );
        }
        else
        {
            //found extension
            if( extension.compare(QLatin1String("m3u")) == 0 || extension.compare(QLatin1String("m3u8")) == 0 )
                return getPlaylist(
                    request,
                    camResource,
                    camera,
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
        m_bytesSent = (size_t)0 - m_writeBuffer.size();

        NX_LOG( QnLog::HTTP_LOG_INDEX, QString::fromLatin1("Sending response to %1:\n%2\n-------------------\n\n\n").
            arg(remoteHostAddress().toString()).
            arg(QString::fromLatin1(m_writeBuffer)), cl_logDEBUG1 );

        m_state = sSending;
    }

    QString formatGUID(const QnUuid& guid)
    {
        QString rez = guid.toString();
        if (rez.startsWith(L'{'))
            return rez;
        else
            return QString(lit("{%1}")).arg(rez);
    }

    bool QnHttpLiveStreamingProcessor::prepareDataToSend()
    {
        Q_ASSERT( m_writeBuffer.isEmpty() );

        if( !m_chunkInputStream )
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
            const int sizeBak = m_writeBuffer.size();
            if( m_chunkInputStream->tryRead( &m_writeBuffer ) )
            {
                NX_LOG( lit("Read %1 bytes from streaming chunk %2").arg(m_writeBuffer.size()-sizeBak).arg((size_t)m_currentChunk.get(), 0, 16), cl_logDEBUG1 );
                return !m_writeBuffer.isEmpty();
            }

            //waiting for data to arrive to chunk
            m_cond.wait( lk.mutex() );
        }
    }

    typedef std::multimap<QString, QString> RequestParamsType;

    nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getPlaylist(
        const nx_http::Request& request,
        const QnSecurityCamResourcePtr& camResource,
        QnVideoCamera* const videoCamera,
        const std::multimap<QString, QString>& requestParams,
        nx_http::Response* const response )
    {
        std::multimap<QString, QString>::const_iterator chunkedParamIter = requestParams.find( StreamingParams::CHUNKED_PARAM_NAME );

        //searching for session (if specified)
        std::multimap<QString, QString>::const_iterator sessionIDIter = requestParams.find( StreamingParams::SESSION_ID_PARAM_NAME );
        const QString& sessionID = sessionIDIter != requestParams.end()
            ? sessionIDIter->second
            : HLSSessionPool::generateUniqueID();

        //session search and add MUST be atomic
        HLSSessionPool::ScopedSessionIDLock lk( HLSSessionPool::instance(), sessionID );

        HLSSession* session = HLSSessionPool::instance()->find( sessionID );
        if( !session )
        {
            std::multimap<QString, QString>::const_iterator hiQualityIter = requestParams.find( StreamingParams::HI_QUALITY_PARAM_NAME );
            std::multimap<QString, QString>::const_iterator loQualityIter = requestParams.find( StreamingParams::LO_QUALITY_PARAM_NAME );
            MediaQuality streamQuality = MEDIA_Quality_None;
            if( chunkedParamIter == requestParams.end() )
            {
                //variant playlist requested
                if( hiQualityIter != requestParams.end() )
                    streamQuality = MEDIA_Quality_High;
                else if( loQualityIter != requestParams.end() )
                    streamQuality = MEDIA_Quality_Low;
                else
                    streamQuality = MEDIA_Quality_Auto;
            }
            else
            {
                //chunked playlist requested
                streamQuality = (hiQualityIter != requestParams.end()) || (loQualityIter == requestParams.end())  //hi quality is default
                    ? MEDIA_Quality_High
                    : MEDIA_Quality_Low;
            }

            if( !camResource->hasDualStreaming() )
            {
                if( streamQuality == MEDIA_Quality_Low )
                {
                    NX_LOG( QString::fromLatin1("Got request to unavailable low quality of camera %2").arg(camResource->getUniqueId()), cl_logDEBUG1 );
                    return nx_http::StatusCode::notFound;
                }
                else if( streamQuality == MEDIA_Quality_Auto )
                {
                    streamQuality = MEDIA_Quality_High;
                }
            }

            const nx_http::StatusCode::Value result = createSession(
                sessionID,
                requestParams,
                camResource,
                videoCamera,
                streamQuality,
                &session );
            if( result != nx_http::StatusCode::ok )
                return result;
            if( !HLSSessionPool::instance()->add( session, DEFAULT_HLS_SESSION_LIVE_TIMEOUT_MS ) )
            {
                assert( false );
            }
        }

        ensureChunkCacheFilledEnoughForPlayback( session, session->streamQuality() );

        if( chunkedParamIter == requestParams.end() )
            return getVariantPlaylist( session, request, camResource, videoCamera, requestParams, response );
        else
            return getChunkedPlaylist( session, request, camResource, requestParams, response );
    }

    nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getVariantPlaylist(
        HLSSession* session,
        const nx_http::Request& request,
        const QnSecurityCamResourcePtr& camResource,
        QnVideoCamera* const videoCamera,
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

        nx_hls::VariantPlaylistData playlistData;
        playlistData.url = baseUrl;
        playlistData.url.setPath( request.requestLine.url.path() );
        //if needed, adding proxy information to playlist url
        nx_http::HttpHeaders::const_iterator viaIter = request.headers.find( "Via" );
        if( viaIter != request.headers.end() )
        {
            nx_http::header::Via via;
            if( !via.parse( viaIter->second ) )
                return nx_http::StatusCode::badRequest;
            if( !via.entries.empty() )
            {
                //TODO #ak check that request has been proxied via media server, not regular Http proxy
                const QString& currentPath = playlistData.url.path();
                playlistData.url.setPath( lit("/proxy/%1/%2").arg(formatGUID(qnCommon->moduleGUID())).
                    arg(currentPath.startsWith(QLatin1Char('/')) ? currentPath.mid(1) : currentPath) );
            }
        }
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

        if( session->streamQuality() == MEDIA_Quality_High || session->streamQuality() == MEDIA_Quality_Auto )
        {
            playlistData.bandwidth = estimateStreamBitrate(
                session,
                camResource,
                videoCamera,
                MEDIA_Quality_High );
            playlistDataQuery.addQueryItem( StreamingParams::HI_QUALITY_PARAM_NAME, QString() );
            playlistData.url.setQuery(playlistDataQuery);
            playlist.playlists.push_back(playlistData);
        }

        if( (session->streamQuality() == MEDIA_Quality_Low || session->streamQuality() == MEDIA_Quality_Auto) && camResource->hasDualStreaming() )
        {
            playlistData.bandwidth = estimateStreamBitrate(
                session,
                camResource,
                videoCamera,
                MEDIA_Quality_Low );
            playlistDataQuery.removeQueryItem( StreamingParams::HI_QUALITY_PARAM_NAME );
            playlistDataQuery.addQueryItem( StreamingParams::LO_QUALITY_PARAM_NAME, QString() );
            playlistData.url.setQuery(playlistDataQuery);
            playlist.playlists.push_back(playlistData);
        }

        if( playlist.playlists.empty() )
            return nx_http::StatusCode::noContent;

        response->messageBody = playlist.toString();
        response->headers.insert( make_pair("Content-Type", "audio/mpegurl") );
        response->headers.insert( make_pair("Content-Length", QByteArray::number(response->messageBody.size()) ) );

        return nx_http::StatusCode::ok;
    }

    nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getChunkedPlaylist(
        HLSSession* const session,
        const nx_http::Request& request,
        const QnSecurityCamResourcePtr& camResource,
        const std::multimap<QString, QString>& requestParams,
        nx_http::Response* const response )
    {
        Q_ASSERT( session );

        std::multimap<QString, QString>::const_iterator hiQualityIter = requestParams.find( StreamingParams::HI_QUALITY_PARAM_NAME );
        std::multimap<QString, QString>::const_iterator loQualityIter = requestParams.find( StreamingParams::LO_QUALITY_PARAM_NAME );
        const MediaQuality streamQuality = (hiQualityIter != requestParams.end()) || (loQualityIter == requestParams.end())  //hi quality is default
            ? MEDIA_Quality_High
            : MEDIA_Quality_Low;

        std::vector<nx_hls::AbstractPlaylistManager::ChunkData> chunkList;
        bool isPlaylistClosed = false;
        const nx_hls::AbstractPlaylistManagerPtr& playlistManager = session->playlistManager(streamQuality);
        if( !playlistManager )
        {
            NX_LOG( QString::fromLatin1("Got request to not available %1 quality of camera %2").
                arg(QLatin1String(streamQuality == MEDIA_Quality_High ? "hi" : "lo")).arg(camResource->getUniqueId()), cl_logWARNING );
            return nx_http::StatusCode::notFound;
        }

        const size_t chunksGenerated = playlistManager->generateChunkList( &chunkList, &isPlaylistClosed );
        if( chunkList.empty() )   //no chunks generated
        {
            NX_LOG( QString::fromLatin1("Failed to get chunks of resource %1").arg(camResource->getUniqueId()), cl_logWARNING );
            return nx_http::StatusCode::noContent;
        }

        NX_LOG( QString::fromLatin1("Prepared playlist of resource %1 (%2 chunks)").arg(camResource->getUniqueId()).arg(chunksGenerated), cl_logDEBUG2 );

        nx_hls::Playlist playlist;
        assert( !chunkList.empty() );
        playlist.mediaSequence = chunkList[0].mediaSequence;
        playlist.closed = isPlaylistClosed;

        //taking parameters, common for every chunks in playlist being generated
        RequestParamsType commonChunkParams;
        for( RequestParamsType::value_type param: requestParams )
        {
            if( param.first == StreamingParams::CHANNEL_PARAM_NAME ||
                param.first == StreamingParams::PICTURE_SIZE_PIXELS_PARAM_NAME ||
                param.first == StreamingParams::CONTAINER_FORMAT_PARAM_NAME ||
                param.first == StreamingParams::VIDEO_CODEC_PARAM_NAME ||
                param.first == StreamingParams::AUDIO_CODEC_PARAM_NAME ||
                param.first == StreamingParams::HI_QUALITY_PARAM_NAME ||
                param.first == StreamingParams::LO_QUALITY_PARAM_NAME ||
                param.first == StreamingParams::SESSION_ID_PARAM_NAME )
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
        baseChunkUrl.setPath( HLS_PREFIX + camResource->getUniqueId() );

        //if needed, adding proxy information to playlist url
        nx_http::HttpHeaders::const_iterator viaIter = request.headers.find( "Via" );
        if( viaIter != request.headers.end() )
        {
            nx_http::header::Via via;
            if( !via.parse( viaIter->second ) )
                return nx_http::StatusCode::badRequest;
            if( !via.entries.empty() )
            {
                //TODO #ak check that request has been proxied via media server, not regular Http proxy
                const QString& currentPath = baseChunkUrl.path();
                baseChunkUrl.setPath( lit("/proxy/%1/%2").arg(formatGUID(qnCommon->moduleGUID())).
                    arg(currentPath.startsWith(QLatin1Char('/')) ? currentPath.mid(1) : currentPath) );
            }
        }

        for( std::vector<nx_hls::AbstractPlaylistManager::ChunkData>::size_type
            i = 0;
            i < chunkList.size();
            ++i )
        {
            nx_hls::Chunk hlsChunk;
            hlsChunk.duration = chunkList[i].duration / (double)USEC_IN_SEC;
            hlsChunk.url = baseChunkUrl;
            QUrlQuery hlsChunkUrlQuery( hlsChunk.url.query() );
            for( RequestParamsType::value_type param: commonChunkParams )
                hlsChunkUrlQuery.addQueryItem( param.first, param.second );
            if( chunkList[i].alias )
            {
                hlsChunkUrlQuery.addQueryItem( StreamingParams::ALIAS_PARAM_NAME, chunkList[i].alias.get() );
                session->saveChunkAlias( streamQuality, chunkList[i].alias.get(), chunkList[i].startTimestamp, chunkList[i].duration );
            }
            else
            {
                hlsChunkUrlQuery.addQueryItem( StreamingParams::START_TIMESTAMP_PARAM_NAME, QString::number(chunkList[i].startTimestamp) );
                hlsChunkUrlQuery.addQueryItem( StreamingParams::DURATION_MS_PARAM_NAME, QString::number(chunkList[i].duration) );
            }
            if( session->isLive() )
                hlsChunkUrlQuery.addQueryItem( StreamingParams::LIVE_PARAM_NAME, QString() );
            hlsChunk.url.setQuery( hlsChunkUrlQuery );
            hlsChunk.discontinuity = chunkList[i].discontinuity;
            hlsChunk.programDateTime = QDateTime::fromMSecsSinceEpoch(chunkList[i].startTimestamp / USEC_IN_MSEC, QTimeZone(QTimeZone::systemTimeZoneId()));
            playlist.chunks.push_back( hlsChunk );
        }

        //playlist.allowCache = !session->isLive(); //TODO: #ak uncomment when done

        response->messageBody = playlist.toString();

        response->headers.insert( make_pair("Content-Type", "audio/mpegurl") );
        response->headers.insert( make_pair("Content-Length", QByteArray::number(response->messageBody.size()) ) );

        return nx_http::StatusCode::ok;
    }

    nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getResourceChunk(
        const nx_http::Request& request,
        const QStringRef& uniqueResourceID,
        const QnSecurityCamResourcePtr& /*camResource*/,
        const std::multimap<QString, QString>& requestParams,
        nx_http::Response* const response )
    {
        std::multimap<QString, QString>::const_iterator hiQualityIter = requestParams.find( StreamingParams::HI_QUALITY_PARAM_NAME );
        std::multimap<QString, QString>::const_iterator loQualityIter = requestParams.find( StreamingParams::LO_QUALITY_PARAM_NAME );
        const MediaQuality streamQuality = (hiQualityIter != requestParams.end()) || (loQualityIter == requestParams.end())  //hi quality is default
            ? MEDIA_Quality_High
            : MEDIA_Quality_Low;

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
        std::multimap<QString, QString>::const_iterator aliasIter = requestParams.find(QLatin1String(StreamingParams::ALIAS_PARAM_NAME));

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
                //converting startDatetime to startTimestamp
                    //this is secondary functionality, not used by this HLS implementation (since all chunks are referenced by npt timestamps)
                startTimestamp = QDateTime::fromString(startDatetimeIter->second, Qt::ISODate).toMSecsSinceEpoch() * USEC_IN_MSEC;
            }
        }
        quint64 chunkDuration = durationIter != requestParams.end()
            ? durationIter->second.toLongLong()
            : nx_ms_conf::DEFAULT_TARGET_DURATION_MS * USEC_IN_MSEC;

        if( aliasIter != requestParams.end() )
        {
            std::multimap<QString, QString>::const_iterator sessionIDIter = requestParams.find( StreamingParams::SESSION_ID_PARAM_NAME );
            const QString& sessionID = sessionIDIter != requestParams.end()
                ? sessionIDIter->second
                : HLSSessionPool::generateUniqueID();
            HLSSessionPool::ScopedSessionIDLock lk( HLSSessionPool::instance(), sessionID );
            HLSSession* session = HLSSessionPool::instance()->find( sessionID );
            if( session )
                session->getChunkByAlias( streamQuality, aliasIter->second, &startTimestamp, &chunkDuration );
        }

        StreamingChunkCacheKey currentChunkKey(
            uniqueResourceID.toString(),
            channelIter != requestParams.end()
                ? channelIter->second.toInt()
                : 0,   //any channel
            containerFormat,
            aliasIter != requestParams.end() ? aliasIter->second : QString(),
            startTimestamp,
            chunkDuration,
            streamQuality,
            requestParams );

        //retrieving streaming chunk
        StreamingChunkPtr chunk;
        if( !StreamingChunkCache::instance()->takeForUse( currentChunkKey, &chunk ) )
        {
            NX_LOG( QString::fromLatin1("Could not get chunk %1 of resource %2 requested by %3").
                arg(request.requestLine.url.query()).arg(uniqueResourceID.toString()).arg(remoteHostAddress().toString()), cl_logDEBUG1 );
            return nx_http::StatusCode::notFound;
        }

        //streaming chunk
        if( m_currentChunk )
        {
            //disconnecting and waiting for already-emitted signals from m_currentChunk to be delivered and processed
            m_currentChunk->disconnectAndJoin( this );
            StreamingChunkCache::instance()->putBackUsedItem( currentChunkKey, m_currentChunk );
            m_currentChunk.reset();
        }

        m_currentChunk = chunk;
        connect(
            m_currentChunk.get(), &StreamingChunk::newDataIsAvailable,
            this,                 &QnHttpLiveStreamingProcessor::chunkDataAvailable,
            Qt::DirectConnection );
        m_chunkInputStream.reset( new StreamingChunkInputStream( m_currentChunk.get() ) );

        //using this simplified test for accept-encoding since hls client do not use syntax with q= ...
        nx_http::header::AcceptEncodingHeader acceptEncoding( nx_http::getHeaderValue( request.headers, "Accept-Encoding" ) );

        response->headers.insert( make_pair( "Content-Type", m_currentChunk->mimeType().toLatin1() ) );
        if( acceptEncoding.encodingIsAllowed("chunked") )
        {
            response->headers.insert( make_pair( "Transfer-Encoding", "chunked" ) );
            response->statusLine.version = nx_http::http_1_1;
            return nx_http::StatusCode::ok;
        }
        else if( acceptEncoding.encodingIsAllowed("identity") )
        {
            //if client requests identity encoding, MUST return identity
            m_currentChunk->waitForChunkReady();    //waiting for chunk to be prepared
            //chunk is ready, using it
            NX_LOG( lit("Streaming chunk %1 with size %2").arg((size_t)m_currentChunk.get(), 0, 16).arg(m_currentChunk->sizeInBytes()), cl_logDEBUG1 );

            auto rangeIter = request.headers.find( "Range" );
            if( rangeIter == request.headers.end() )
            {
                response->headers.insert( make_pair( "Transfer-Encoding", "identity" ) );
                response->headers.insert( make_pair( "Content-Length", nx_http::StringType::number((qlonglong)m_currentChunk->sizeInBytes()) ) );
                response->statusLine.version = request.requestLine.version; //do not require HTTP/1.1 here
                return nx_http::StatusCode::ok;
            }

            //partial content request
            response->statusLine.version = nx_http::http_1_1;   //Range is supported by HTTP/1.1
            nx_http::header::Range range;
            if( !range.parse( rangeIter->second ) || !range.validateByContentSize(m_currentChunk->sizeInBytes()) )
            {
                response->headers.insert( make_pair( "Content-Range", "*/"+nx_http::StringType::number((quint64)m_currentChunk->sizeInBytes()) ) );
                m_chunkInputStream.reset();
                return nx_http::StatusCode::rangeNotSatisfiable;
            }

            response->headers.insert( make_pair( "Transfer-Encoding", "identity" ) );
            response->headers.insert( make_pair( "Content-Range", rangeIter->second+"/"+nx_http::StringType::number((quint64)m_currentChunk->sizeInBytes()) ) );
            response->headers.insert( make_pair( "Content-Length", nx_http::StringType::number(range.totalRangeLength(m_currentChunk->sizeInBytes())) ) );

            static_cast<StreamingChunkInputStream*>(m_chunkInputStream.get())->setByteRange( range );
            return nx_http::StatusCode::partialContent;
        }
        else
        {
            m_chunkInputStream.reset();
            return nx_http::StatusCode::notAcceptable;
        }
    }

    nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::createSession(
        const QString& sessionID,
        const std::multimap<QString, QString>& requestParams,
        const QnSecurityCamResourcePtr& camResource,
        QnVideoCamera* const videoCamera,
        MediaQuality streamQuality,
        HLSSession** session )
    {
        std::vector<MediaQuality> requiredQualities;
        requiredQualities.reserve( 2 );
        if( streamQuality == MEDIA_Quality_High || streamQuality == MEDIA_Quality_Auto )
            requiredQualities.push_back( MEDIA_Quality_High );
        if( streamQuality == MEDIA_Quality_Low || streamQuality == MEDIA_Quality_Auto )
            requiredQualities.push_back( MEDIA_Quality_Low );

        boost::optional<quint64> startTimestamp;
        std::multimap<QString, QString>::const_iterator startTimestampIter = requestParams.find(StreamingParams::START_TIMESTAMP_PARAM_NAME);
        if( startTimestampIter != requestParams.end() )
        {
            startTimestamp = startTimestampIter->second.toULongLong();
        }
        else
        {
            std::multimap<QString, QString>::const_iterator startDatetimeIter = requestParams.find(StreamingParams::START_DATETIME_PARAM_NAME);
            if( startDatetimeIter != requestParams.end() )
                startTimestamp = QDateTime::fromString(startDatetimeIter->second, Qt::ISODate).toMSecsSinceEpoch() * USEC_IN_MSEC;
        }

        std::unique_ptr<HLSSession> newHlsSession(
            new HLSSession(
                sessionID,
                MSSettings::roSettings()->value( nx_ms_conf::HLS_TARGET_DURATION_MS, nx_ms_conf::DEFAULT_TARGET_DURATION_MS).toUInt(),
                !startTimestamp,   //if no start date specified, providing live stream
                streamQuality,
                videoCamera ) );
        if( newHlsSession->isLive() )
        {
            //LIVE session
            //starting live caching, if it is not started
            for( const MediaQuality quality: requiredQualities )
            {
                if( !videoCamera->ensureLiveCacheStarted(quality, newHlsSession->targetDurationMS() * USEC_IN_MSEC) )
                {
                    NX_LOG( QString::fromLatin1("Error. Requested live hls playlist of resource %1 with no live cache").arg(camResource->getUniqueId()), cl_logDEBUG1 );
                    return nx_http::StatusCode::noContent;
                }
                assert( videoCamera->hlsLivePlaylistManager(quality) );
                newHlsSession->setPlaylistManager( quality, videoCamera->hlsLivePlaylistManager(quality) );
            }
        }
        else
        {
            //converting startDatetime to timestamp
            for( const MediaQuality quality: requiredQualities )
            {
                //generating sliding playlist, holding not more than CHUNK_COUNT_IN_ARCHIVE_PLAYLIST archive chunks
                nx_hls::ArchivePlaylistManagerPtr archivePlaylistManager = 
                    std::make_shared<ArchivePlaylistManager>(
                        camResource,
                        startTimestamp.get(),
                        CHUNK_COUNT_IN_ARCHIVE_PLAYLIST,
                        newHlsSession->targetDurationMS() * USEC_IN_MSEC,
                        quality );
                if( !archivePlaylistManager->initialize() )
                {
                    NX_LOG( QString::fromLatin1("QnHttpLiveStreamingProcessor::getPlaylist. Failed to initialize archive playlist for camera %1").
                        arg(camResource->getUniqueId()), cl_logDEBUG1 );
                    return nx_http::StatusCode::internalServerError;
                }

                newHlsSession->setPlaylistManager( quality, archivePlaylistManager );
            }
        }

        *session = newHlsSession.release();
        return nx_http::StatusCode::ok;
    }

    int QnHttpLiveStreamingProcessor::estimateStreamBitrate(
        HLSSession* const session,
        QnSecurityCamResourcePtr camResource,
        QnVideoCamera* const videoCamera,
        MediaQuality streamQuality )
    {
        int bandwidth = session->playlistManager(streamQuality)->getMaxBitrate();
        if( (bandwidth == -1) && videoCamera->liveCache(streamQuality) )
            bandwidth = videoCamera->liveCache(streamQuality)->getMaxBitrate();
        if( bandwidth == -1 )
        {
            //estimating bitrate as we can
            QnConstCompressedVideoDataPtr videoFrame = videoCamera->getLastVideoFrame( streamQuality == MEDIA_Quality_High, 0);
            if( videoFrame )
                bandwidth = videoFrame->dataSize() * CHAR_BIT / COMMON_KEY_FRAME_TO_NON_KEY_FRAME_RATIO * camResource->getMaxFps();
        }
        if( bandwidth == -1 )
            bandwidth = DEFAULT_PRIMARY_STREAM_BITRATE;
        return bandwidth;
    }

    void QnHttpLiveStreamingProcessor::chunkDataAvailable( StreamingChunkPtr /*chunk*/, quint64 /*newSizeBytes*/ )
    {
        QMutexLocker lk( &m_mutex );
        m_cond.wakeAll();
    }

    void QnHttpLiveStreamingProcessor::ensureChunkCacheFilledEnoughForPlayback( HLSSession* const session, MediaQuality streamQuality )
    {
        static const size_t MIN_PLAYLIST_SIZE_TO_START_STREAMING = 1;
        static const size_t PLAYLIST_CHECK_TIMEOUT_MS = 1000;

        if( !session->isLive() )
            return; //TODO #ak investigate archive streaming (likely, there is no such problem there)

        //TODO #ak proper handling of MEDIA_Quality_Auto
        if( streamQuality == MEDIA_Quality_Auto )
            streamQuality = MEDIA_Quality_High;

        //if no chunks in cache, waiting for cache to be filled
        std::vector<nx_hls::AbstractPlaylistManager::ChunkData> chunkList;
        bool isPlaylistClosed = false;
        size_t chunksGenerated = session->playlistManager(streamQuality)->generateChunkList( &chunkList, &isPlaylistClosed );
        if( chunksGenerated == 0 )
        {
            //no chunks generated, waiting for at least one chunk to be generated
            QElapsedTimer monotonicTimer;
            monotonicTimer.restart();
            while( (quint64)monotonicTimer.elapsed() < session->targetDurationMS() * (MIN_PLAYLIST_SIZE_TO_START_STREAMING + 2) )
            {
                chunkList.clear();
                chunksGenerated = session->playlistManager(streamQuality)->generateChunkList( &chunkList, &isPlaylistClosed );
                if( chunksGenerated >= MIN_PLAYLIST_SIZE_TO_START_STREAMING )
                    break;
                QThread::msleep( PLAYLIST_CHECK_TIMEOUT_MS );
            }
        }
    }
}
