// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_way_audio_streamer.h"

#include <api/network_proxy_factory.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/media/audio_data_packet.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/websocket/websocket.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_data_provider_base.h>

static constexpr int kMaxBufferLength = 100;

namespace nx::vms::client::core {

TwoWayAudioStreamer::TwoWayAudioStreamer(std::unique_ptr<QnAbstractStreamDataProvider> provider):
    m_provider(std::move(provider))
{}

bool TwoWayAudioStreamer::start(
    const nx::network::http::Credentials& credentials, const QnVirtualCameraResourcePtr& camera)
{
    m_httpClient = std::make_unique<nx::network::http::AsyncClient>(
        nx::network::ssl::kAcceptAnyCertificate);

    auto server = camera->getParentServer();
    nx::network::SocketAddress address = server->getPrimaryAddress();
    QNetworkProxy proxy = QnNetworkProxyFactory::proxyToResource(server);
    if (proxy.type() != QNetworkProxy::NoProxy)
    {
        NX_DEBUG(this, "Proxy access through server %1:%2", proxy.hostName(), proxy.port());
        address = nx::network::SocketAddress(proxy.hostName(), (uint16_t) proxy.port());
    }

    if (m_useWebsocket)
    {
        nx::network::http::HttpHeaders httpHeaders;
        nx::network::websocket::addClientHeaders(&httpHeaders,
            nx::network::websocket::kWebsocketProtocolName,
            nx::network::websocket::CompressionType::none);
        m_httpClient->setAdditionalHeaders(httpHeaders);
    }
    m_httpClient->setCredentials(credentials);
    m_httpClient->addAdditionalHeader(
        Qn::SERVER_GUID_HEADER_NAME, server->getId().toStdString(QUuid::WithBraces));

    QUrlQuery query;
    query.addQueryItem("camera_id", camera->getId().toString());
    query.addQueryItem("live", "true");
    //query.addQueryItem("access-token", QString::fromStdString(credentials.authToken.value));

    const auto url = nx::network::url::Builder()
        .setScheme("https")
        .setEndpoint(address)
        .setPath("/api/http_audio")
        .setQuery(query)
        .toUrl();

    NX_DEBUG(this, "Make output audio request: %1", url.toString());

    if (m_useWebsocket)
    {
        m_httpClient->doUpgrade(url,
            nx::network::http::Method::get,
            nx::network::websocket::kWebsocketProtocolName,
            [this, self = shared_from_this()] () mutable { onUpgradeHttpClient(); });
    }
    else
    {
        m_httpClient->setOnRequestHasBeenSent(
            [this, self = shared_from_this()] (bool /*isRetryAfterUnauthorizedResponse*/) mutable { onHttpRequest(); });
        m_httpClient->doPost(url, [] () mutable { });
    }
    return true;
}

TwoWayAudioStreamer::~TwoWayAudioStreamer()
{
    if (m_provider)
        m_provider->removeDataProcessor(this);

    if (m_httpClient)
        m_httpClient->pleaseStopSync();

    if (m_streamChannel)
        m_streamChannel->cancelIOSync(network::aio::EventType::etAll);
}

void TwoWayAudioStreamer::stop()
{
    if (m_provider)
    {
        auto provider = dynamic_cast<DesktopDataProviderBase*>(m_provider.get());
        if (provider)
            provider->pleaseStopSync(); //< Flush audio encoder.

        m_provider->removeDataProcessor(this);
        m_provider.reset();
    }
}

void TwoWayAudioStreamer::onHttpRequest()
{
    auto socket = m_httpClient->takeSocket();
    if (!socket)
    {
        NX_DEBUG(this, "Http client has empty socket");
        return;
    }

    socket->setNonBlockingMode(true);
    m_streamChannel = nx::network::aio::makeAsyncChannelAdapter(std::move(socket));
    m_httpClient->setOnRequestHasBeenSent(nullptr);
    startProvider();
}

void TwoWayAudioStreamer::onUpgradeHttpClient()
{
    if (m_httpClient->failed()
        || (m_useWebsocket && m_httpClient->response()->statusLine.statusCode
        != nx::network::http::StatusCode::switchingProtocols))
    {
        auto code = m_httpClient->response() ?
            m_httpClient->response()->statusLine.statusCode :
            nx::network::http::StatusCode::undefined;
        NX_ERROR(this, "Server connection code: %1, error: %2",
            code, m_httpClient->lastSysErrorCode());
        return;
    }

    auto socket = m_httpClient->takeSocket();
    m_httpClient.reset();

    if (!socket)
    {
        NX_DEBUG(this, "Http client has empty socket");
        return;
    }

    socket->setNonBlockingMode(true);
    auto webSocket = std::make_unique<nx::network::websocket::WebSocket>(
        std::move(socket),
        nx::network::websocket::Role::server,
        nx::network::websocket::FrameType::binary,
        nx::network::websocket::CompressionType::none);
    webSocket->start();
    m_streamChannel = std::move(webSocket);
    startProvider();
}

void TwoWayAudioStreamer::startProvider()
{
    m_provider->subscribe(this);
    m_provider->startIfNotRunning();
}

void TwoWayAudioStreamer::onDataSent()
{
    {
        auto bufferQueue = m_bufferQueue.lock();
        if (bufferQueue->empty())
        {
            m_sendInProcess = false;
            return;
        }
        auto data = std::move(bufferQueue->front());
        bufferQueue->pop_front();
        m_dataBuffer.assign(data->data(), data->size());
    }
    m_streamChannel->sendAsync(&m_dataBuffer,
        [this, self = shared_from_this()](SystemError::ErrorCode errorCode, std::size_t bytesSent)
        {
            if (nx::network::socketCannotRecoverFromError(errorCode) || bytesSent == 0)
            {
                NX_DEBUG(this, "Socket send data error: %1, bytes sent: %2", errorCode, bytesSent);
                return;
            }
            onDataSent();
        });
}

bool TwoWayAudioStreamer::canAcceptData() const
{
    return true;
}

bool TwoWayAudioStreamer::initializeMuxer(const QnCompressedAudioDataPtr& data)
{
    FfmpegMuxer::Config muxerConfig;
    m_muxer = std::make_unique<FfmpegMuxer>(muxerConfig);
    if (!m_muxer->setContainer("mp3"))
        return false;

    if (!m_muxer->addAudio(data->context->getAvCodecParameters()))
        return false;

    return m_muxer->open();
}

void TwoWayAudioStreamer::putData(const QnAbstractDataPacketPtr& data)
{
    auto audioData = std::dynamic_pointer_cast<QnCompressedAudioData>(data);
    if (!audioData)
    {
        NX_VERBOSE(this, "Skip non-audio packet");
        return;
    }

    NX_VERBOSE(this, "Process audio data: %1", audioData);
    if ((int)m_bufferQueue.lock()->size() >= kMaxBufferLength)
    {
        NX_DEBUG(this, "Drop audio packet: %1, buffer is full", audioData->timestamp);
        return;
    }

    if (!m_muxer && !initializeMuxer(audioData))
    {
        NX_WARNING(this, "Failed to intialize audio muxer");
        m_muxer.reset();
        return;
    }

    if (!m_muxer->process(audioData))
    {
        NX_WARNING(this, "Failed to mux audio data to container: %1", audioData);
        return;
    }

    auto buffer = std::make_unique<nx::utils::ByteArray>();
    m_muxer->getResult(buffer.get());

    if (buffer->size() == 0)
        return;

    m_bufferQueue.lock()->push_back(std::move(buffer));
    if (!m_sendInProcess)
    {
        m_sendInProcess = true;
        onDataSent();
    }
}

void TwoWayAudioStreamer::clearUnprocessedData()
{
    m_bufferQueue.lock()->clear();
}

} // namespace nx::vms::client::core
