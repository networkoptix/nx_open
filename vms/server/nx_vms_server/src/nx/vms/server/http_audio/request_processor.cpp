
#include "request_processor.h"

#include <functional>
#include <thread>

#include <nx/vms/api/protocol_version.h>
#include <nx/network/websocket/websocket_handshake.h>


// Streaming from camera
#include <nx/vms/server/resource/camera.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/server/http_audio/http_audio_consumer.h>
#include <camera/camera_pool.h>

// Streaming to camera
#include <streaming/audio_streamer_pool.h>
#include <network/tcp_connection_priv.h>
#include <nx/vms/server/http_audio/http_audio_provider.h>

namespace nx::vms::server::http_audio {

class SocketToAsyncChannelAdapter : public nx::network::aio::AbstractAsyncChannel
{
public:
    SocketToAsyncChannelAdapter(std::unique_ptr<nx::network::AbstractCommunicatingSocket> socket) :
        m_socket(std::move(socket))
    {
    }

    virtual void readSomeAsync(
        nx::Buffer* const buffer, nx::network::IoCompletionHandler handler) override
    {
        m_socket->readSomeAsync(buffer, std::move(handler));
    }

    virtual void sendAsync(
        const nx::Buffer& /*buffer*/, nx::network::IoCompletionHandler /*handler*/) override
    {
        return;
    }

    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override
    {
        m_socket->cancelIOSync(eventType);
    }

private:
    std::unique_ptr<nx::network::AbstractCommunicatingSocket> m_socket;
};

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

QnVideoCameraPtr AudioRequestProcessor::getVideoCamera(const QnUuid& cameraId)
{
    auto resource = serverModule()->resourcePool()->getResourceById<resource::Camera>(cameraId);
    if (!resource)
    {
        NX_WARNING(this, "Resource not found %1", cameraId);
        return QnVideoCameraPtr();
    }

    if (!resource->isInitialized())
    {
        NX_DEBUG(this,
            "Trying to initialize resource if it was not initialized for some unknown reason");
        resource->init();
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
    auto audioConsumer = std::make_unique<HttpAudioConsumer>(std::move(socket));
    liveReader->addDataProcessor(audioConsumer.get());
    liveReader->startIfNotRunning();
    audioConsumer->start();

    while (!needToStop() && audioConsumer->isRunning())
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    liveReader->removeDataProcessor(audioConsumer.get());
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
    QString cameraIdStr = query.queryItemValue("camera_id");
    QnUuid cameraId = QnUuid::fromStringSafe(cameraIdStr);
    if (cameraId.isNull())
    {
        NX_WARNING(this, "Invalid camera id specified: '%1'", cameraIdStr);
        sendResponse(http::StatusCode::badRequest, http::StringType());
        return;
    }

    nx::network::aio::AsyncChannelPtr streamChannel;
    d->socket->setNonBlockingMode(true);
    unsigned int millis = 0;
    d->socket->getRecvTimeout(&millis);
    NX_DEBUG(this, "recv timeout: %1", millis);
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
        streamChannel = std::make_unique<SocketToAsyncChannelAdapter>(std::move(d->socket));
    }

    auto videoCamera = getVideoCamera(cameraId);
    if (!videoCamera)
    {
        sendResponse(http::StatusCode::notFound, http::StringType());
        return;
    }

    QSharedPointer<HttpAudioProvider> httpProvider(
        new HttpAudioProvider(std::move(streamChannel)));
    FfmpegAudioDemuxer::StreamConfig config;
    config.format = query.queryItemValue("format").toStdString();
    config.sampleRate = query.queryItemValue("sample_rate").toStdString();
    config.channelsNumber = query.queryItemValue("channels").toStdString();
    if (!httpProvider->openStream(config.format.empty() ? nullptr : &config))
    {
        NX_WARNING(this, "Failed to open audio stream for camera: %1", cameraId);
        return;
    }

    QString errorMessage;
    bool result = audioStreamPool()->startStopStreamToResource(
        httpProvider, cameraId, QnAudioStreamerPool::Action::Start, errorMessage);
    if (!result)
    {
        NX_WARNING(this, "Failed to start streaming audio to camera: %1, error: %2",
            cameraId, errorMessage);
        return;
    }

    videoCamera->inUse(this);
    while (httpProvider->isRunning() && !needToStop())
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    //audioStreamPool()->startStopStreamToResource(
      //  httpProvider, cameraId, QnAudioStreamerPool::Action::Stop, errorMessage);
    httpProvider->pleaseStop();
    videoCamera->notInUse(this);
}

} // namespace nx::vms::server::http_audio
