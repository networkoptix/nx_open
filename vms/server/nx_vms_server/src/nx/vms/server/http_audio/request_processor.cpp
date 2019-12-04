
#include "request_processor.h"

#include <functional>
#include <thread>

#include <nx/vms/api/protocol_version.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <api/helpers/camera_id_helper.h>

// Streaming from camera
#include <nx/vms/server/resource/camera.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/server/http_audio/async_channel_audio_consumer.h>
#include <camera/camera_pool.h>

// Streaming to camera
#include <streaming/audio_streamer_pool.h>
#include <network/tcp_connection_priv.h>
#include <nx/vms/server/http_audio/async_channel_audio_provider.h>
#include <nx/network/aio/async_channel_adapter.h>

namespace {
    constexpr std::chrono::milliseconds kIdleTime {10};
}

namespace nx::vms::server::http_audio {

AudioRequestProcessor::AudioRequestProcessor(
    QnMediaServerModule* serverModule,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnTcpListener* owner)
    :
    QnTCPConnectionProcessor(std::move(socket), owner),
    ServerModuleAware(serverModule)
{
    setObjectName(::toString(this));
}

AudioRequestProcessor::~AudioRequestProcessor()
{
    NX_DEBUG(this, "Http audio request closed");
    stop();
}

QnVideoCameraPtr AudioRequestProcessor::getVideoCamera(const QString& cameraId)
{
    QnResourcePtr resource = nx::camera_id_helper::findCameraByFlexibleId(
        commonModule()->resourcePool(), cameraId);

    if (!resource)
    {
        NX_WARNING(this, "Resource not found %1", cameraId);
        return QnVideoCameraPtr();
    }

    if (!resource->isInitialized())
    {
        NX_DEBUG(this,
            "Trying to initialize resource if it was not initialized for some unknown reason");
        if (!resource->init())
        {
            NX_WARNING(this, "Failed to initialize camera resource %1", cameraId);
            return QnVideoCameraPtr();
        }
        if (!resource->isInitialized())
        {
            NX_WARNING(this, "Camera still not initialized, resource %1", cameraId);
            return QnVideoCameraPtr();
        }
    }
    nx::vms::server::resource::Camera* camRes =
        dynamic_cast<nx::vms::server::resource::Camera*>(resource.get());
    if (!camRes || !camRes->hasCameraCapabilities(Qn::AudioTransmitCapability))
    {
        NX_WARNING(this, "Camera does not support backchannel audio, resource %1", cameraId);
        return QnVideoCameraPtr();
    }

    auto camera = videoCameraPool()->getVideoCamera(resource);
    if (!camera)
    {
        NX_WARNING(this, "Video camera not found %1", resource);
        return QnVideoCameraPtr();
    }
    return camera;
}

void AudioRequestProcessor::startAudioStreaming(
    nx::network::aio::AsyncChannelPtr socket, const QnVideoCameraPtr& camera)
{
    auto liveReader = camera->getLiveReader(QnServer::HiQualityCatalog);
    if (!liveReader)
    {
        NX_WARNING(this, "Failed to obtain live stream reader %1", camera);
        return;
    }
    auto audioConsumer = std::make_unique<AsyncChannelAudioConsumer>(std::move(socket));
    liveReader->addDataProcessor(audioConsumer.get());
    liveReader->startIfNotRunning();
    audioConsumer->start();

    while (!needToStop() && audioConsumer->isRunning())
        std::this_thread::sleep_for(kIdleTime);

    liveReader->removeDataProcessor(audioConsumer.get());
}


void AudioRequestProcessor::startAudioReceiving(
    nx::network::aio::AsyncChannelPtr streamChannel,
    const QnVideoCameraPtr& camera,
    const QUrlQuery& query)
{
    std::optional<FfmpegAudioDemuxer::StreamConfig> config;
    if (!query.queryItemValue("format").toStdString().empty())
    {
        config.emplace();
        config.value().format = query.queryItemValue("format").toStdString();
        config.value().sampleRate = query.queryItemValue("sample_rate").toStdString();
        config.value().channelsNumber = query.queryItemValue("channels").toStdString();
    }

    QSharedPointer<AsyncChannelAudioProvider> httpProvider(
        new AsyncChannelAudioProvider(std::move(streamChannel), config));

    QString errorMessage;
    bool result = audioStreamPool()->startStopStreamToResource(
        httpProvider,
        camera->resource()->getId(),
        QnAudioStreamerPool::Action::Start,
        errorMessage);
    if (!result)
    {
        NX_WARNING(this, "Failed to start streaming audio to camera: %1, error: %2",
            camera->resource(), errorMessage);
        return;
    }

    VideoCameraLocker locker(camera);
    while (httpProvider->isRunning() && !needToStop())
        std::this_thread::sleep_for(kIdleTime);

    httpProvider->pleaseStop();
}

void AudioRequestProcessor::run()
{
    using namespace nx::network;
    Q_D(QnTCPConnectionProcessor);

    NX_DEBUG(this, "New http audio request");
    initSystemThreadId();

    if (d->clientRequest.isEmpty()/* && !readRequest()*/)
        return;

    parseRequest();
    auto query = QUrlQuery(d->request.requestLine.url.query().toLower());
    QString cameraId = query.queryItemValue("camera_id");
    auto videoCamera = getVideoCamera(cameraId);
    if (!videoCamera)
    {
        NX_WARNING(this, "Invalid video camera %1", cameraId);
        sendResponse(http::StatusCode::notFound, http::StringType());
        return;
    }

    nx::network::aio::AsyncChannelPtr streamChannel;
    d->socket->setNonBlockingMode(true);
    auto error = websocket::validateRequest(d->request, &d->response, true /*disableCompression*/);
    if (error == websocket::Error::noError) // websocket
    {
        sendResponse(http::StatusCode::switchingProtocols, http::StringType());
        const auto dataFormat = websocket::FrameType::binary;
        const auto compressionType = websocket::CompressionType::none;

        auto webSocket = std::make_unique<websocket::WebSocket>(
            std::move(d->socket), websocket::Role::server, dataFormat, compressionType);
        webSocket->start();
        streamChannel = std::move(webSocket);
    }
    else // http
    {
        // TODO
        //sendResponse(http::StatusCode::ok, nx::network::http::StringType());
        streamChannel = nx::network::aio::makeAsyncChannelAdapter(std::move(d->socket));
    }

    startAudioReceiving(std::move(streamChannel), videoCamera, query);
}

} // namespace nx::vms::server::http_audio
