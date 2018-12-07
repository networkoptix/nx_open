#include "hls_server.h"

#include <QtCore/QUrlQuery>
#include <QtCore/QTimeZone>
#include <QtCore/QCoreApplication>

#include <algorithm>
#include <limits>
#include <map>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>

#include <nx/network/http/http_content_type.h>
#include <nx/network/hls/hls_types.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/log/log.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/string.h>
#include <nx/utils/system_error.h>
#include <utils/media/ffmpeg_helper.h>
#include <utils/media/av_codec_helper.h>
#include <utils/media/media_stream_cache.h>
#include <utils/common/app_info.h>

#include "camera/camera_pool.h"
#include "hls_archive_playlist_manager.h"
#include "hls_live_playlist_manager.h"
#include "hls_playlist_manager_proxy.h"
#include "hls_session_pool.h"
#include "media_server/settings.h"
#include "streaming/streaming_chunk_cache.h"
#include "streaming/streaming_params.h"
#include <network/tcp_connection_priv.h>
#include <network/tcp_listener.h>
#include <media_server/media_server_module.h>
#include <rest/server/json_rest_result.h>
#include <nx/fusion/serialization_format.h>
#include <api/helpers/camera_id_helper.h>
#include <network/universal_tcp_listener.h>
#include <plugins/resource/server_archive/server_archive_delegate.h>
#include <nx/metrics/metrics_storage.h>

using namespace nx::network::http;

namespace nx::vms::server::hls {

static constexpr size_t kReadBufferSize = 64 * 1024;
static constexpr int kChunkCountInArchivePlaylist = 3;
static constexpr char kHlsPrefix[] = "/hls/";
static constexpr quint64 kMillisPerSec = 1000;
static constexpr quint64 kUsecPerMs = 1000;
static constexpr quint64 kUsecPerSec = kMillisPerSec * kUsecPerMs;
static constexpr auto kDefaultHlsSessionLiveTimeout =
    nx::vms::server::Settings::kDefaultHlsTargetDurationMs * 7;
static constexpr int kCommonKeyFrameToNonKeyFrameRatio = 5;
static constexpr int kDefaultPrimaryStreamBitrate = 4 * 1024 * 1024;

size_t HttpLiveStreamingProcessor::m_minPlaylistSizeToStartStreaming =
    nx::vms::server::Settings::kDefaultHlsPlaylistPreFillChunks;

HttpLiveStreamingProcessor::HttpLiveStreamingProcessor(
    QnMediaServerModule* serverModule,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnTcpListener* owner)
    :
    QnTCPConnectionProcessor(std::move(socket), owner),
    nx::vms::server::ServerModuleAware(serverModule),
    m_state(State::receiving),
    m_switchToChunkedTransfer(false),
    m_useChunkedTransfer(false),
    m_bytesSent(0)
{
    setObjectName("HttpLiveStreamingProcessor");
}

void HttpLiveStreamingProcessor::setMinPlayListSizeToStartStreaming(size_t value)
{
    m_minPlaylistSizeToStartStreaming = value;
}

HttpLiveStreamingProcessor::~HttpLiveStreamingProcessor()
{
    // While we are here only HttpLiveStreamingProcessor::chunkDataAvailable slot can be called.

    if (m_currentChunk)
    {
        // Disconnecting and waiting for already-emitted signals from m_currentChunk
        // to be delivered and processed.
        // TODO #ak cancel on-going transcoding. Currently, it just wastes CPU time.
        m_chunkInputStream.reset();
        m_currentChunk.reset();
    }

    // TODO/HLS: #ak Clean up archive chunk data, since we do not cache archive chunks.
    // This should be done automatically by cache: should mark archive chunks as "uncachable".
}

void HttpLiveStreamingProcessor::processRequest(
    const nx::network::http::Request& request)
{
    Q_D(QnTCPConnectionProcessor);

    nx::network::http::Response response;
    response.statusLine.version = request.requestLine.version;

    QnJsonRestResult error;
    response.statusLine.statusCode = getRequestedFile(request, &response, &error);
    if (response.statusLine.statusCode == nx::network::http::StatusCode::forbidden)
    {
        sendUnauthorizedResponse(nx::network::http::StatusCode::forbidden);
        m_state = State::done;
        return;
    }
    else if (error.error != QnJsonRestResult::NoError)
    {
        d->response.messageBody = QJson::serialized(error);
        base_type::sendResponse(
            nx::network::http::StatusCode::ok,
            Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::JsonFormat));
        m_state = State::done;
        return;
    }

    prepareResponse(request, &response);
    sendResponse(response);
}

void HttpLiveStreamingProcessor::prepareResponse(
    const nx::network::http::Request& request,
    nx::network::http::Response* const response)
{
    if (response->statusLine.reasonPhrase.isEmpty())
    {
        response->statusLine.reasonPhrase =
            StatusCode::toString(response->statusLine.statusCode);
    }

    const auto currentTimeInHttpFormat =
        QLocale(QLocale::English).toString(
            QDateTime::currentDateTime(),
            lit("ddd, d MMM yyyy hh:mm:ss t")).toLatin1();

    response->headers.emplace("Date", currentTimeInHttpFormat);
    response->headers.emplace(nx::network::http::header::Server::NAME, nx::network::http::serverString());
    response->headers.emplace("Cache-Control", "no-cache");
    response->headers.emplace("Access-Control-Allow-Origin", "*");

    if (request.requestLine.version == nx::network::http::http_1_1)
    {
        if (nx::network::http::StatusCode::isSuccessCode(response->statusLine.statusCode) &&
            (response->headers.find("Transfer-Encoding") == response->headers.end()) &&
            (response->headers.find("Content-Length") == response->headers.end()))
        {
            response->headers.emplace("Transfer-Encoding", "chunked");
        }
        response->headers.emplace("Connection", "close");
    }

    if (response->statusLine.statusCode == nx::network::http::StatusCode::notFound)
    {
        nx::network::http::insertOrReplaceHeader(
            &response->headers,
            nx::network::http::HttpHeader("Content-Length", "0"));
    }
}

void HttpLiveStreamingProcessor::run()
{
    Q_D(QnTCPConnectionProcessor);

    auto metrics = commonModule()->metrics();
    metrics->tcpConnections().hls()++;
    auto metricsGuard = nx::utils::makeScopeGuard(
        [metrics]() { metrics->tcpConnections().hls()--; });

    while (!needToStop())
    {
        switch (m_state)
        {
            case State::receiving:
                if (!readSingleRequest())
                {
                    NX_WARNING(this, lm("Error reading/parsing request from %1. "
                        "Terminating connection...").args(remoteHostAddress().toString()));
                    m_state = State::done;
                    break;
                }

                m_state = State::processingMessage;
                m_useChunkedTransfer = false;
                processRequest(d->request);
                break;

            case State::processingMessage:
                NX_ASSERT(false);
                break;

            case State::sending:
            {
                NX_ASSERT(!m_writeBuffer.isEmpty());

                int bytesSent = 0;
                if (m_useChunkedTransfer)
                    bytesSent = sendChunk(m_writeBuffer) ? m_writeBuffer.size() : -1;
                else
                    bytesSent = sendData(m_writeBuffer) ? m_writeBuffer.size() : -1;
                if (bytesSent < 0)
                {
                    NX_WARNING(this, lm("Error sending data to %1 (%2). Sent %3 bytes total. "
                        "Terminating connection...").args(remoteHostAddress().toString(),
                            SystemError::getLastOSErrorText(), m_bytesSent));
                    m_state = State::done;
                    break;
                }
                else
                {
                    m_bytesSent += bytesSent;
                }

                if (bytesSent == m_writeBuffer.size())
                    m_writeBuffer.clear();
                else
                    m_writeBuffer.remove(0, bytesSent);
                if (!m_writeBuffer.isEmpty())
                    break;  //< Continuing sending.

                if (!prepareDataToSend())
                {
                    NX_DEBUG(this, lm("Finished uploading %1 data to %2. Sent %3 bytes total. Closing connection...")
                        .args(m_currentFileName, remoteHostAddress().toString(), m_bytesSent));
                    // Sending empty chunk to signal EOF.
                    if (m_useChunkedTransfer)
                        sendChunk(QByteArray());
                    m_state = State::done;
                    break;
                }
                break;
            }

            case State::done:
                NX_DEBUG(QnLog::HTTP_LOG_INDEX,
                    lm("Done message to %1 (sent %2 bytes). Closing connection...\n\n\n")
                        .args(remoteHostAddress().toString(), m_bytesSent));
                return;
        }
    }
}

nx::network::http::StatusCode::Value HttpLiveStreamingProcessor::getRequestedFile(
    const nx::network::http::Request& request,
    nx::network::http::Response* const response,
    QnJsonRestResult* error)
{
    //retreiving requested file name
    const QString& path = request.requestLine.url.path();
    int fileNameStartIndex = path.lastIndexOf(QChar('/'));
    if (fileNameStartIndex == -1)
    {
        NX_DEBUG(this, lm("HLS. Not found file name in path %1").args(path));
        return StatusCode::notFound;
    }

    QStringRef fileName;
    if (fileNameStartIndex == path.size() - 1)   //path ends with /. E.g., /hls/camera1.m3u/. Skipping trailing /
    {
        int newFileNameStartIndex = path.midRef(0, path.size() - 1).lastIndexOf(QChar('/'));
        if (newFileNameStartIndex == -1)
        {
            NX_DEBUG(this, lm("HLS. Not found file name (2) in path %1").arg(path));
            return StatusCode::notFound;
        }
        fileName = path.midRef(newFileNameStartIndex + 1, fileNameStartIndex - (newFileNameStartIndex + 1));
    }
    else
    {
        fileName = path.midRef(fileNameStartIndex + 1);
    }
    m_currentFileName = fileName.toString();

    const int extensionSepPos = fileName.lastIndexOf(QChar('.'));
    const QStringRef& extension =
        extensionSepPos != -1 ? fileName.mid(extensionSepPos + 1) : QStringRef();
    const QStringRef& shortFileName = fileName.mid(0, extensionSepPos);

    // Searching for requested resource.
    const QString& resId = shortFileName.toString();
    QnResourcePtr resource = nx::camera_id_helper::findCameraByFlexibleId(
        serverModule()->resourcePool(), resId);
    if (!resource)
    {
        NX_DEBUG(this, lm("HLS. Requested resource %1 not found").arg(resId));
        return nx::network::http::StatusCode::notFound;
    }

    //parsing request parameters
    const auto& queryItemsList = QUrlQuery(request.requestLine.url.query()).queryItems();
    std::multimap<QString, QString> requestParams;
    //moving params to map for more convenient use
    for (auto it = queryItemsList.begin();
        it != queryItemsList.end();
        ++it)
    {
        requestParams.insert(std::make_pair(it->first, it->second));
    }

    QnSecurityCamResourcePtr camResource = resource.dynamicCast<QnSecurityCamResource>();
    if (!camResource)
    {
        NX_DEBUG(this, lm("HLS. Requested resource %1 is not a camera").arg(shortFileName));
        return nx::network::http::StatusCode::notFound;
    }

    //checking resource stream type. Only h.264 is OK for HLS
    QnVideoCameraPtr camera = serverModule()->videoCameraPool()->getVideoCamera(camResource);
    if (!camera)
    {
        NX_VERBOSE(this, lm("Error. HLS request to resource %1 which is not a camera")
            .args(camResource->getUniqueId()));
        return nx::network::http::StatusCode::forbidden;
    }

    QnConstCompressedVideoDataPtr lastVideoFrame = camera->getLastVideoFrame(
        (requestParams.find(StreamingParams::LO_QUALITY_PARAM_NAME) == requestParams.end())
        ? Qn::StreamIndex::primary
        : Qn::StreamIndex::secondary,
        /*channel*/ 0);
    if (lastVideoFrame &&
        (lastVideoFrame->compressionType != AV_CODEC_ID_H264) &&
        (lastVideoFrame->compressionType != AV_CODEC_ID_NONE))
    {
        //video is not in h.264 format
        NX_WARNING(this, lm("Error. HLS request to resource %1 with codec %2")
            .args(camResource->getUniqueId(),
                QnAvCodecHelper::codecIdToString(lastVideoFrame->compressionType)));
        return nx::network::http::StatusCode::forbidden;
    }

    if (extension.compare(QLatin1String("m3u")) == 0 ||
        extension.compare(QLatin1String("m3u8")) == 0)
    {
        return getPlaylist(
            request,
            extension.toString(),
            camResource,
            d_ptr->accessRights,
            camera,
            requestParams,
            response,
            error);
    }
    else
    {
        // Chunk requsted, checking requested container format...
        QString containerFormat;
        auto containerIter = requestParams.find(StreamingParams::CONTAINER_FORMAT_PARAM_NAME);
        if (containerIter != requestParams.end())
        {
            containerFormat = containerIter->second;
        }
        else
        {
            //detecting container format by extension
            if (extension.isEmpty() || extension == "ts")
                containerFormat = "mpegts";
            else if (extension == "mkv")
                containerFormat = "matroska";
            else if (extension == "mp4")
                containerFormat = "mp4";
        }

        if (containerFormat == "mpegts" ||
            //containerFormat == "mp4" ||      //TODO #ak ffmpeg: muxer does not support unseekable output
            containerFormat == "matroska")     //some supported format has been requested
        {
            if (containerIter == requestParams.end())
                requestParams.emplace(StreamingParams::CONTAINER_FORMAT_PARAM_NAME, containerFormat);
            return getResourceChunk(
                request,
                shortFileName,
                camResource,
                requestParams,
                response);
        }
    }

    NX_DEBUG(this, lm("HLS. Unknown file type has been requested: \"%1\"")
        .args(extension.toString()));
    return StatusCode::notFound;
}

void HttpLiveStreamingProcessor::sendResponse(const nx::network::http::Response& response)
{
    //serializing response to internal buffer
    const auto it = response.headers.find("Transfer-Encoding");
    if (it != response.headers.end() && it->second == "chunked")
        m_switchToChunkedTransfer = true;
    m_writeBuffer.clear();
    response.serialize(&m_writeBuffer);
    m_bytesSent = (size_t)0 - m_writeBuffer.size();

    NX_DEBUG(QnLog::HTTP_LOG_INDEX, lm("Sending response to %1:\n%2\n-------------------\n\n\n")
        .args(remoteHostAddress().toString(), QString::fromLatin1(m_writeBuffer)));

    m_state = State::sending;
}

bool HttpLiveStreamingProcessor::prepareDataToSend()
{
    static const int kMaxBytesToRead = 1024 * 1024;

    NX_ASSERT(m_writeBuffer.isEmpty());

    if (!m_chunkInputStream)
        return false;

    if (m_switchToChunkedTransfer)
    {
        m_useChunkedTransfer = true;
        m_switchToChunkedTransfer = false;
    }

    for (;; )
    {
        // Reading chunk data.
        const int sizeBak = m_writeBuffer.size();
        if (m_chunkInputStream->tryRead(&m_writeBuffer, kMaxBytesToRead))
        {
            NX_DEBUG(this, lm("Read %1 bytes from streaming chunk %2")
                .args(m_writeBuffer.size() - sizeBak, m_currentChunk.get()));
            return !m_writeBuffer.isEmpty();
        }

        // Waiting for data to arrive to chunk.
        m_chunkInputStream->waitForSomeDataAvailable();
    }
}

const char* HttpLiveStreamingProcessor::mimeTypeByExtension(const QString& extension) const
{
    if (extension.toLower() == "m3u8")
        return nx::network::http::kApplicationMpegUrlMimeType;

    return nx::network::http::kAudioMpegUrlMimeType;
}

typedef std::multimap<QString, QString> RequestParamsType;

nx::network::http::StatusCode::Value HttpLiveStreamingProcessor::getPlaylist(
    const nx::network::http::Request& request,
    const QString& requestFileExtension,
    const QnSecurityCamResourcePtr& camResource,
    const Qn::UserAccessData& accessRights,
    const QnVideoCameraPtr& videoCamera,
    const std::multimap<QString, QString>& requestParams,
    nx::network::http::Response* const response,
    QnJsonRestResult* error)
{
    auto chunkedParamIter = requestParams.find(StreamingParams::CHUNKED_PARAM_NAME);

    //searching for session (if specified)
    auto sessionIDIter = requestParams.find(StreamingParams::SESSION_ID_PARAM_NAME);
    const QString& sessionID = sessionIDIter != requestParams.end()
        ? sessionIDIter->second
        : SessionPool::generateUniqueID();

    //session search and add MUST be atomic
    SessionPool::ScopedSessionIDLock lk(serverModule()->hlsSessionPool(), sessionID);

    Session* session = serverModule()->hlsSessionPool()->find(sessionID);
    if (!session)
    {
        auto hiQualityIter = requestParams.find(StreamingParams::HI_QUALITY_PARAM_NAME);
        auto loQualityIter = requestParams.find(StreamingParams::LO_QUALITY_PARAM_NAME);
        MediaQuality streamQuality = MEDIA_Quality_None;
        if (chunkedParamIter == requestParams.end())
        {
            //variant playlist requested
            if (hiQualityIter != requestParams.end())
                streamQuality = MEDIA_Quality_High;
            else if (loQualityIter != requestParams.end())
                streamQuality = MEDIA_Quality_Low;
            else
                streamQuality = MEDIA_Quality_Auto;
        }
        else
        {
            //chunked playlist requested
            streamQuality =
                (hiQualityIter != requestParams.end()) || (loQualityIter == requestParams.end())
                ? MEDIA_Quality_High
                : MEDIA_Quality_Low;
        }

        if (!camResource->hasDualStreaming())
        {
            if (streamQuality == MEDIA_Quality_Low)
            {
                NX_DEBUG(this, lm("Got request to unavailable low quality of camera %2")
                    .args(camResource->getUniqueId()));
                return nx::network::http::StatusCode::notFound;
            }
            else if (streamQuality == MEDIA_Quality_Auto)
            {
                streamQuality = MEDIA_Quality_High;
            }
        }

        const nx::network::http::StatusCode::Value result = createSession(
            accessRights,
            request.requestLine.url.path(),
            sessionID,
            requestParams,
            camResource,
            videoCamera,
            streamQuality,
            &session,
            error);
        if (result != nx::network::http::StatusCode::ok || error->error)
            return result;
        
        if (!serverModule()->hlsSessionPool()->add(
                std::unique_ptr<Session>(session), kDefaultHlsSessionLiveTimeout))
        {
            NX_ASSERT(false);
        }
    }

    auto requiredPermission = session->isLive()
        ? Qn::Permission::ViewLivePermission : Qn::Permission::ViewFootagePermission;
    if (!serverModule()->resourceAccessManager()->hasPermission(accessRights, camResource, requiredPermission))
    {
        if (serverModule()->resourceAccessManager()->hasPermission(
            accessRights, camResource, Qn::Permission::ViewLivePermission))
        {
            error->errorString = toString(Qn::MediaStreamEvent::ForbiddenWithNoLicense);
            error->error = QnRestResult::Forbidden;
            return nx::network::http::StatusCode::ok;
        }
        return nx::network::http::StatusCode::forbidden;
    }

    ensureChunkCacheFilledEnoughForPlayback(session, session->streamQuality());

    QByteArray serializedPlaylist;
    if (chunkedParamIter == requestParams.end())
    {
        response->statusLine.statusCode = getVariantPlaylist(
            session, request, camResource, videoCamera, requestParams, &serializedPlaylist);
    }
    else
    {
        response->statusLine.statusCode = getChunkedPlaylist(
            session, request, camResource, requestParams, &serializedPlaylist);
    }

    if (response->statusLine.statusCode != nx::network::http::StatusCode::ok)
        return static_cast<nx::network::http::StatusCode::Value>(response->statusLine.statusCode);

    response->messageBody = serializedPlaylist;
    response->headers.emplace("Content-Type", mimeTypeByExtension(requestFileExtension));
    response->headers.emplace("Content-Length", QByteArray::number(response->messageBody.size()));

    return nx::network::http::StatusCode::ok;
}

nx::network::http::StatusCode::Value HttpLiveStreamingProcessor::getVariantPlaylist(
    Session* session,
    const nx::network::http::Request& request,
    const QnSecurityCamResourcePtr& camResource,
    const QnVideoCameraPtr& videoCamera,
    const std::multimap<QString, QString>& /*requestParams*/,
    QByteArray* serializedPlaylist)
{
    network::hls::VariantPlaylist playlist;

    nx::utils::Url baseUrl;

    network::hls::VariantPlaylistData playlistData;
    playlistData.url = baseUrl;
    playlistData.url.setPath(request.requestLine.url.path());
    //if needed, adding proxy information to playlist url
    auto viaIter = request.headers.find("Via");
    if (viaIter != request.headers.end())
    {
        nx::network::http::header::Via via;
        if (!via.parse(viaIter->second))
            return nx::network::http::StatusCode::badRequest;
    }
    QList<QPair<QString, QString> > queryItems =
        QUrlQuery(request.requestLine.url.query()).queryItems();
    //removing SESSION_ID_PARAM_NAME
    for (auto it = queryItems.begin();
        it != queryItems.end();
        )
    {
        if ((it->first == StreamingParams::CHUNKED_PARAM_NAME) || (it->first == StreamingParams::SESSION_ID_PARAM_NAME))
            queryItems.erase(it++);
        else
            ++it;
    }

    QUrlQuery playlistDataQuery;
    queryItems.push_front(session->playlistAuthenticationQueryItem());
    playlistDataQuery.setQueryItems(queryItems);
    playlistDataQuery.addQueryItem(StreamingParams::CHUNKED_PARAM_NAME, QString());
    playlistDataQuery.addQueryItem(StreamingParams::SESSION_ID_PARAM_NAME, session->id());

    //adding hi stream playlist
    if (session->streamQuality() == MEDIA_Quality_High || session->streamQuality() == MEDIA_Quality_Auto)
    {
        playlistData.bandwidth = estimateStreamBitrate(
            session,
            camResource,
            videoCamera,
            MEDIA_Quality_High);
        playlistDataQuery.addQueryItem(StreamingParams::HI_QUALITY_PARAM_NAME, QString());
        playlistData.url.setQuery(playlistDataQuery);
        playlist.playlists.push_back(playlistData);
    }

    //adding lo stream playlist
    if ((session->streamQuality() == MEDIA_Quality_Low || session->streamQuality() == MEDIA_Quality_Auto) &&
        camResource->hasDualStreaming())
    {
        playlistData.bandwidth = estimateStreamBitrate(
            session,
            camResource,
            videoCamera,
            MEDIA_Quality_Low);
        playlistDataQuery.removeQueryItem(StreamingParams::HI_QUALITY_PARAM_NAME);
        playlistDataQuery.addQueryItem(StreamingParams::LO_QUALITY_PARAM_NAME, QString());
        playlistData.url.setQuery(playlistDataQuery);
        playlist.playlists.push_back(playlistData);
    }

    if (playlist.playlists.empty())
        return nx::network::http::StatusCode::noContent;

    *serializedPlaylist = playlist.toString();
    return nx::network::http::StatusCode::ok;
}

nx::network::http::StatusCode::Value HttpLiveStreamingProcessor::getChunkedPlaylist(
    Session* const session,
    const nx::network::http::Request& request,
    const QnSecurityCamResourcePtr& camResource,
    const std::multimap<QString, QString>& requestParams,
    QByteArray* serializedPlaylist)
{
    NX_ASSERT(session);

    auto hiQualityIter = requestParams.find(StreamingParams::HI_QUALITY_PARAM_NAME);
    auto loQualityIter = requestParams.find(StreamingParams::LO_QUALITY_PARAM_NAME);
    const MediaQuality streamQuality =
        (hiQualityIter != requestParams.end()) || (loQualityIter == requestParams.end())  //hi quality is default
        ? MEDIA_Quality_High
        : MEDIA_Quality_Low;

    std::vector<hls::AbstractPlaylistManager::ChunkData> chunkList;
    bool isPlaylistClosed = false;
    const hls::AbstractPlaylistManagerPtr& playlistManager = session->playlistManager(streamQuality);
    if (!playlistManager)
    {
        NX_WARNING(this, lm("Got request to not available %1 quality of camera %2")
            .args(QLatin1String(streamQuality == MEDIA_Quality_High ? "hi" : "lo"),
                camResource->getUniqueId()));
        return nx::network::http::StatusCode::notFound;
    }

    const size_t chunksGenerated = playlistManager->generateChunkList(&chunkList, &isPlaylistClosed);
    if (chunkList.empty())   //no chunks generated
    {
        NX_WARNING(this, lm("Failed to get chunks of resource %1")
            .args(camResource->getUniqueId()));
        return nx::network::http::StatusCode::noContent;
    }

    NX_VERBOSE(this, lm("Prepared playlist of resource %1 (%2 chunks)")
        .args(camResource->getUniqueId()).arg(chunksGenerated));

    nx::network::hls::Playlist playlist;
    NX_ASSERT(!chunkList.empty());
    playlist.mediaSequence = chunkList[0].mediaSequence;
    playlist.closed = isPlaylistClosed;

    //taking parameters, common for every chunks in playlist being generated
    RequestParamsType commonChunkParams;
    for (RequestParamsType::value_type param : requestParams)
    {
        if (param.first == StreamingParams::CHANNEL_PARAM_NAME ||
            param.first == StreamingParams::PICTURE_SIZE_PIXELS_PARAM_NAME ||
            param.first == StreamingParams::CONTAINER_FORMAT_PARAM_NAME ||
            param.first == StreamingParams::VIDEO_CODEC_PARAM_NAME ||
            param.first == StreamingParams::AUDIO_CODEC_PARAM_NAME ||
            param.first == StreamingParams::HI_QUALITY_PARAM_NAME ||
            param.first == StreamingParams::LO_QUALITY_PARAM_NAME ||
            param.first == StreamingParams::SESSION_ID_PARAM_NAME)
        {
            commonChunkParams.insert(param);
        }
    }

    nx::utils::Url baseChunkUrl;
    baseChunkUrl.setPath(kHlsPrefix + camResource->getUniqueId() + ".ts");

    //if needed, adding proxy information to playlist url
    auto viaIter = request.headers.find("Via");
    if (viaIter != request.headers.end())
    {
        nx::network::http::header::Via via;
        if (!via.parse(viaIter->second))
            return nx::network::http::StatusCode::badRequest;
    }

    const auto chunkAuthenticationQueryItem = session->chunkAuthenticationQueryItem();
    for (std::vector<hls::AbstractPlaylistManager::ChunkData>::size_type
        i = 0;
        i < chunkList.size();
        ++i)
    {
        network::hls::Chunk hlsChunk;
        hlsChunk.duration = chunkList[i].duration / (double)kUsecPerSec;
        hlsChunk.url = baseChunkUrl;
        QUrlQuery hlsChunkUrlQuery(hlsChunk.url.query());
        hlsChunkUrlQuery.addQueryItem(
            chunkAuthenticationQueryItem.first,
            chunkAuthenticationQueryItem.second);
        for (RequestParamsType::value_type param : commonChunkParams)
            hlsChunkUrlQuery.addQueryItem(param.first, param.second);
        if (chunkList[i].alias)
        {
            hlsChunkUrlQuery.addQueryItem(StreamingParams::ALIAS_PARAM_NAME, chunkList[i].alias.get());
            session->saveChunkAlias(
                streamQuality,
                chunkList[i].alias.get(),
                chunkList[i].startTimestamp,
                chunkList[i].duration);
        }
        else
        {
            hlsChunkUrlQuery.addQueryItem(
                StreamingParams::START_TIMESTAMP_PARAM_NAME,
                QString::number(chunkList[i].startTimestamp));

            hlsChunkUrlQuery.addQueryItem(
                StreamingParams::DURATION_USEC_PARAM_NAME,
                QString::number(chunkList[i].duration));
        }
        if (session->isLive())
            hlsChunkUrlQuery.addQueryItem(StreamingParams::LIVE_PARAM_NAME, QString());

        hlsChunk.url.setQuery(hlsChunkUrlQuery);
        hlsChunk.discontinuity = chunkList[i].discontinuity;
        hlsChunk.programDateTime = QDateTime::fromMSecsSinceEpoch(
            chunkList[i].startTimestamp / kUsecPerMs,
            QTimeZone(QTimeZone::systemTimeZoneId()));
        playlist.chunks.push_back(hlsChunk);
    }

    *serializedPlaylist = playlist.toString();
    return nx::network::http::StatusCode::ok;
}

nx::network::http::StatusCode::Value HttpLiveStreamingProcessor::getResourceChunk(
    const nx::network::http::Request& request,
    const QStringRef& uniqueResourceID,
    const QnSecurityCamResourcePtr& cameraResource,
    const std::multimap<QString, QString>& requestParams,
    nx::network::http::Response* const response)
{
    RequestParams params = readRequestParams(requestParams);

    quint64 startTimestamp = 0;
    if (params.startTimestamp)
        startTimestamp = *params.startTimestamp;

    quint64 chunkDuration =
        nx::vms::server::Settings::kDefaultHlsTargetDurationMs.count() * kUsecPerMs;
    if (params.duration)
        chunkDuration = params.duration->count();

    StreamingChunkCacheKey currentChunkKey(
        uniqueResourceID.toString(),
        params.channel,
        params.containerFormat,
        params.alias ? *params.alias : QString(),
        startTimestamp,
        std::chrono::microseconds(chunkDuration),
        params.streamQuality,
        requestParams);

    bool requestIsAPartOfHlsSession = false;
    auto sessionIDIter = requestParams.find(StreamingParams::SESSION_ID_PARAM_NAME);
    if (sessionIDIter != requestParams.end())
    {
        SessionPool::ScopedSessionIDLock lk(serverModule()->hlsSessionPool(), sessionIDIter->second);
        Session* hlsSession = serverModule()->hlsSessionPool()->find(sessionIDIter->second);
        if (hlsSession)
        {
            requestIsAPartOfHlsSession = true;
            hlsSession->updateAuditInfo(startTimestamp);
            if (params.alias)
            {
                hlsSession->getChunkByAlias(
                    params.streamQuality, *params.alias, &startTimestamp, &chunkDuration);
            }

            if (!hlsSession->audioCodecId())
                hlsSession->setAudioCodecId(detectAudioCodecId(currentChunkKey));
            currentChunkKey.setAudioCodecId(*hlsSession->audioCodecId());
        }
    }
    else
    {
        currentChunkKey.setAudioCodecId(detectAudioCodecId(currentChunkKey));
    }

    auto requiredPermission = currentChunkKey.live()
        ? Qn::Permission::ViewLivePermission : Qn::Permission::ViewFootagePermission;
    if (!serverModule()->resourceAccessManager()->hasPermission(
        d_ptr->accessRights, cameraResource, requiredPermission))
    {
        return nx::network::http::StatusCode::forbidden;
    }

    //streaming chunk
    if (m_currentChunk)
    {
        //disconnecting and waiting for already-emitted signals from m_currentChunk to be delivered and processed
        m_currentChunk.reset();
    }

    //retrieving streaming chunk
    StreamingChunkPtr chunk;
    m_chunkInputStream = serverModule()->streamingChunkCache()->getChunkForReading(
        currentChunkKey, &chunk);
    if (!m_chunkInputStream)
    {
        NX_DEBUG(this, lm("Could not get chunk %1 of resource %2 requested by %3")
            .arg(request.requestLine.url.query()).arg(uniqueResourceID.toString())
            .arg(remoteHostAddress().toString()));
        return nx::network::http::StatusCode::notFound;
    }
    m_currentChunk = chunk;

    //using this simplified test for accept-encoding since hls client do not use syntax with q= ...
    const auto acceptEncodingHeaderIter = request.headers.find("Accept-Encoding");
    nx::network::http::header::AcceptEncodingHeader acceptEncoding(
        acceptEncodingHeaderIter == request.headers.end()
        ? nx::network::http::StringType()
        : acceptEncodingHeaderIter->second);

    //in case of hls enabling caching of full chunk since it may be required by hls client
    if (requestIsAPartOfHlsSession)
        m_currentChunk->disableInternalBufferLimit();

    response->headers.insert(std::make_pair("Content-Type", m_currentChunk->mimeType().toLatin1()));
    if (acceptEncoding.encodingIsAllowed("chunked")
        || (acceptEncodingHeaderIter == request.headers.end()
            && request.requestLine.version == nx::network::http::http_1_1))  //if no Accept-Encoding then it is supported by HTTP/1.1
    {
        response->headers.insert(std::make_pair("Transfer-Encoding", "chunked"));
        response->statusLine.version = nx::network::http::http_1_1;
        return nx::network::http::StatusCode::ok;
    }
    else if (acceptEncoding.encodingIsAllowed("identity"))
    {
        //if chunk exceeds maximum allowed size then proving
        //  it in streaming mode. That means no Content-Length in response
        const bool chunkCompleted = m_currentChunk->waitForChunkReadyOrInternalBufferFilled();

        //chunk is ready, using it
        NX_DEBUG(this, lm("Streaming %1 chunk %2 with size %3")
            .args(chunkCompleted ? "complete" : "incomplete",
                m_currentChunk.get(), m_currentChunk->sizeInBytes()));

        auto rangeIter = request.headers.find("Range");
        if (rangeIter == request.headers.end() || !chunkCompleted)
        {
            // < If whole chunk does not fit in memory then disabling partial request support.
            if (chunkCompleted)
            {
                response->headers.insert(std::make_pair(
                    "Content-Length",
                    nx::network::http::StringType::number((qlonglong)m_currentChunk->sizeInBytes())));
            }
            else
            {
                // This means end-of-file will be signalled by closing connection.
                response->headers.insert(std::make_pair("Transfer-Encoding", "identity"));
            }
            response->statusLine.version = request.requestLine.version; //do not require HTTP/1.1 here
            return nx::network::http::StatusCode::ok;
        }

        //partial content request
        response->statusLine.version = nx::network::http::http_1_1;   //Range is supported by HTTP/1.1
        nx::network::http::header::Range range;
        nx::network::http::header::ContentRange contentRange;
        contentRange.instanceLength = (quint64)m_currentChunk->sizeInBytes();
        if (!range.parse(rangeIter->second) || !range.validateByContentSize(m_currentChunk->sizeInBytes()))
        {
            response->headers.emplace(
                "Content-Range", contentRange.toString());
            response->headers.emplace(
                "Content-Length",
                nx::network::http::StringType::number(contentRange.rangeLength()));

            m_chunkInputStream.reset();
            return nx::network::http::StatusCode::rangeNotSatisfiable;
        }

        //range is satisfiable
        if (range.rangeSpecList.size() > 0)
            contentRange.rangeSpec = range.rangeSpecList.front();

        response->headers.emplace("Content-Range", contentRange.toString());
        response->headers.emplace(
            "Content-Length",
            nx::network::http::StringType::number(contentRange.rangeLength()));

        static_cast<StreamingChunkInputStream*>(m_chunkInputStream.get())->setByteRange(contentRange);
        return nx::network::http::StatusCode::partialContent;
    }
    else
    {
        m_chunkInputStream.reset();
        return nx::network::http::StatusCode::notAcceptable;
    }
}

nx::network::http::StatusCode::Value HttpLiveStreamingProcessor::createSession(
    const Qn::UserAccessData& accessRights,
    const QString& requestedPlaylistPath,
    const QString& sessionID,
    const std::multimap<QString, QString>& requestParams,
    const QnSecurityCamResourcePtr& camResource,
    const QnVideoCameraPtr& videoCamera,
    MediaQuality streamQuality,
    Session** session,
    QnJsonRestResult* error)
{
    Qn::MediaStreamEvent mediaStreamEvent = Qn::MediaStreamEvent::NoEvent;
    auto startDatetimeIter = requestParams.find(QLatin1String(StreamingParams::START_POS_PARAM_NAME));
    if (camResource && startDatetimeIter == requestParams.end())
        mediaStreamEvent = camResource->checkForErrors(); //< Check errors for LIVE only.
    if (mediaStreamEvent)
    {
        error->errorString = toString(mediaStreamEvent);
        error->error = QnRestResult::Forbidden;
        return nx::network::http::StatusCode::ok;
    }

    std::vector<MediaQuality> requiredQualities;
    requiredQualities.reserve(2);
    if (streamQuality == MEDIA_Quality_High || streamQuality == MEDIA_Quality_Auto)
        requiredQualities.push_back(MEDIA_Quality_High);
    if ((streamQuality == MEDIA_Quality_Low) ||
        (streamQuality == MEDIA_Quality_Auto && camResource->hasDualStreaming()))
    {
        requiredQualities.push_back(MEDIA_Quality_Low);
    }

    RequestParams params = readRequestParams(requestParams);

    using namespace std::chrono;

    std::unique_ptr<Session> newHlsSession(
        new Session(
            serverModule(),
            sessionID,
            serverModule()->settings().hlsTargetDurationMS(),
            !params.startTimestamp,   //if no start date specified, providing live stream
            streamQuality,
            videoCamera,
            authSession()));
    if (newHlsSession->isLive())
    {
        //LIVE session
        //starting live caching, if it is not started
        for (const MediaQuality quality : requiredQualities)
        {
            if (!videoCamera->ensureLiveCacheStarted(
                quality,
                duration_cast<microseconds>(newHlsSession->targetDuration()).count()))
            {
                NX_DEBUG(this,
                    lm("Error. Requested live hls playlist of resource %1 with no live cache")
                        .args(camResource->getUniqueId()));
                return nx::network::http::StatusCode::noContent;
            }
            NX_ASSERT(videoCamera->hlsLivePlaylistManager(quality));
            newHlsSession->setPlaylistManager(
                quality,
                std::make_shared<hls::PlayListManagerWeakRefProxy>(
                    videoCamera->hlsLivePlaylistManager(quality)));
        }
    }
    else
    {
        //converting startDatetime to timestamp
        for (const MediaQuality quality: requiredQualities)
        {
            //generating sliding playlist, holding not more than kChunkCountInArchivePlaylist archive chunks
            hls::ArchivePlaylistManagerPtr archivePlaylistManager =
                std::make_shared<ArchivePlaylistManager>(
                    serverModule(),
                    camResource,
                    params.startTimestamp.get(),
                    kChunkCountInArchivePlaylist,
                    duration_cast<microseconds>(newHlsSession->targetDuration()),
                    quality);
            if (!archivePlaylistManager->initialize())
            {
                mediaStreamEvent = archivePlaylistManager->lastError().toMediaStreamEvent();
                NX_DEBUG(this,
                    lm("QnHttpLiveStreamingProcessor::getPlaylist. "
                        "Failed to initialize archive playlist for camera %1")
                    .args(camResource->getUniqueId(), toString(mediaStreamEvent)));
                if (mediaStreamEvent)
                {
                    error->errorString = toString(mediaStreamEvent);
                    error->error = QnRestResult::Forbidden;
                    return nx::network::http::StatusCode::ok;
                }
                return nx::network::http::StatusCode::internalServerError;
            }

            newHlsSession->setPlaylistManager(quality, archivePlaylistManager);
        }
    }

    const auto authenticator = QnUniversalTcpListener::authenticator(owner());
    newHlsSession->setChunkAuthenticationQueryItem(authenticator->makeQueryItemForPath(
        accessRights, kHlsPrefix + camResource->getUniqueId() + ".ts"));

    newHlsSession->setPlaylistAuthenticationQueryItem(authenticator->makeQueryItemForPath(
        accessRights, requestedPlaylistPath));

    *session = newHlsSession.release();
    return nx::network::http::StatusCode::ok;
}

int HttpLiveStreamingProcessor::estimateStreamBitrate(
    Session* const session,
    QnSecurityCamResourcePtr camResource,
    const QnVideoCameraPtr& videoCamera,
    MediaQuality streamQuality)
{
    int bandwidth = session->playlistManager(streamQuality)->getMaxBitrate();
    if ((bandwidth == -1) && videoCamera->liveCache(streamQuality))
        bandwidth = videoCamera->liveCache(streamQuality)->getMaxBitrate();
    if (bandwidth == -1)
    {
        //estimating bitrate as we can
        QnConstCompressedVideoDataPtr videoFrame = videoCamera->getLastVideoFrame(
            (streamQuality == MEDIA_Quality_High)
            ? Qn::StreamIndex::primary
            : Qn::StreamIndex::secondary,
            /*channel*/0);
        if (videoFrame)
        {
            bandwidth = (int)(videoFrame->dataSize() * CHAR_BIT / kCommonKeyFrameToNonKeyFrameRatio *
                camResource->getMaxFps());
        }
    }
    if (bandwidth == -1)
        bandwidth = kDefaultPrimaryStreamBitrate;
    return bandwidth;
}

void HttpLiveStreamingProcessor::ensureChunkCacheFilledEnoughForPlayback(
    Session* const session,
    MediaQuality streamQuality)
{
    static const size_t PLAYLIST_CHECK_TIMEOUT_MS = 1000;

    if (!session->isLive())
        return; //TODO #ak investigate archive streaming (likely, there is no such problem there)

    //TODO #ak proper handling of MEDIA_Quality_Auto
    if (streamQuality == MEDIA_Quality_Auto)
        streamQuality = MEDIA_Quality_High;

    //if no chunks in cache, waiting for cache to be filled
    std::vector<hls::AbstractPlaylistManager::ChunkData> chunkList;
    bool isPlaylistClosed = false;
    size_t chunksGenerated = session->playlistManager(streamQuality)
        ->generateChunkList(&chunkList, &isPlaylistClosed);
    if (chunksGenerated < m_minPlaylistSizeToStartStreaming)
    {
        //no chunks generated, waiting for at least one chunk to be generated
        QElapsedTimer monotonicTimer;
        monotonicTimer.restart();
        while ((quint64)monotonicTimer.elapsed() < session->targetDuration().count() * (m_minPlaylistSizeToStartStreaming + 2))
        {
            chunkList.clear();
            chunksGenerated = session->playlistManager(streamQuality)
                ->generateChunkList(&chunkList, &isPlaylistClosed);
            if (chunksGenerated >= m_minPlaylistSizeToStartStreaming)
            {
                NX_VERBOSE(this, lm("HLS cache has been prefilled with %1 chunks")
                    .args(chunksGenerated));
                break;
            }
            QThread::msleep(PLAYLIST_CHECK_TIMEOUT_MS);
        }
    }
}

AVCodecID HttpLiveStreamingProcessor::detectAudioCodecId(
    const StreamingChunkCacheKey& chunkParams)
{
    const auto resource = nx::camera_id_helper::findCameraByFlexibleId(
        serverModule()->resourcePool(),
        chunkParams.srcResourceUniqueID());
    if (!resource)
        return AV_CODEC_ID_NONE;

    QnServerArchiveDelegate archive(serverModule());
    if (!archive.open(resource, serverModule()->archiveIntegrityWatcher()))
        return AV_CODEC_ID_NONE;
    if (chunkParams.startTimestamp() != DATETIME_NOW)
        archive.seek(chunkParams.startTimestamp(), true);
    if (archive.getAudioLayout() &&
        archive.getAudioLayout()->getAudioTrackInfo(0).codecContext)
    {
        return archive.getAudioLayout()->getAudioTrackInfo(0).codecContext->getCodecId();
    }

    return AV_CODEC_ID_NONE;
}

RequestParams HttpLiveStreamingProcessor::readRequestParams(
    const std::multimap<QString, QString>& requestParams)
{
    RequestParams result;

    auto hiQualityIter = requestParams.find(StreamingParams::HI_QUALITY_PARAM_NAME);
    auto loQualityIter = requestParams.find(StreamingParams::LO_QUALITY_PARAM_NAME);
    result.streamQuality =
        (hiQualityIter != requestParams.end()) || (loQualityIter == requestParams.end())  //hi quality is default
        ? MEDIA_Quality_High
        : MEDIA_Quality_Low;

    auto channelIter = requestParams.find(StreamingParams::CHANNEL_PARAM_NAME);
    if (channelIter != requestParams.end())
        result.channel = channelIter->second.toInt();
    auto containerIter = requestParams.find(StreamingParams::CONTAINER_FORMAT_PARAM_NAME);
    if (containerIter != requestParams.end())
        result.containerFormat = containerIter->second;

    auto startTimestampIter =
        requestParams.find(StreamingParams::START_TIMESTAMP_PARAM_NAME);

    if (startTimestampIter != requestParams.end())
    {
        result.startTimestamp = startTimestampIter->second.toULongLong();
    }
    else
    {
        auto startDatetimeIter = requestParams.find(StreamingParams::START_POS_PARAM_NAME);
        if (startDatetimeIter != requestParams.end())
        {
            // Converting startDatetime to startTimestamp.
            // This is secondary functionality, not used by this
            //   HLS implementation (since all chunks are referenced by npt timestamps).
            result.startTimestamp = nx::utils::parseDateTime(startDatetimeIter->second);
        }
        else
        {
            // Trying compatibility parameter "startDatetime".
            auto startDatetimeIter = requestParams.find(StreamingParams::START_DATETIME_PARAM_NAME);
            if (startDatetimeIter != requestParams.end())
                result.startTimestamp = nx::utils::parseDateTime(startDatetimeIter->second);
        }
    }

    auto durationUSecIter = requestParams.find(StreamingParams::DURATION_USEC_PARAM_NAME);
    auto durationSecIter = requestParams.find(StreamingParams::DURATION_SEC_PARAM_NAME);

    if (durationUSecIter != requestParams.end())
        result.duration = std::chrono::microseconds(durationUSecIter->second.toLongLong());
    else if (durationSecIter != requestParams.end())
        result.duration = std::chrono::seconds(durationSecIter->second.toLongLong());

    auto aliasIter = requestParams.find(StreamingParams::ALIAS_PARAM_NAME);
    if (aliasIter != requestParams.end())
        result.alias = aliasIter->second;

    return result;
}

} // namespace nx::vms::server::hls
