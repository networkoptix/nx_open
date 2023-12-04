// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_camera_connection.h"

#include <QtCore/QElapsedTimer>

#include <client_core/client_core_module.h>
#include <core/dataprovider/data_provider_factory.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/tcp_connection_priv.h>
#include <nx/media/config.h>
#include <nx/network/buffered_stream_socket.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_client.h>
#include <nx/network/ssl/helpers.h>
#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/common/system_context.h>
#include <rtsp/rtsp_ffmpeg_encoder.h>

#include "desktop_resource.h"

static const int CONNECT_TIMEOUT = 1000 * 5;
static const int KEEP_ALIVE_TIMEOUT = 1000 * 120;

/** Corresponds to the same-name Server setting. */
static const int kMaxTcpRequestSize = 512 * 1024 * 1024;

namespace nx::vms::client::core {

class DesktopCameraConsumer: public QnAbstractDataConsumer
{
public:
    DesktopCameraConsumer(DesktopCameraConnectionProcessor* owner):
        QnAbstractDataConsumer(50),
        m_owner(owner),
        m_needVideoData(false)
    {
        for (int i = 0; i < 2; ++i)
        {
            m_serializers[i].reset(new QnRtspFfmpegEncoder(
                DecoderConfig(),
                owner->systemContext()->metrics().get()));
            m_serializers[i]->setAdditionFlags(0);
            m_serializers[i]->setLiveMarker(true);
            m_serializers[i]->setServerVersion(owner->serverVersion());
        }
    }

    virtual ~DesktopCameraConsumer()
    {
        stop();
    }

    void setNeedVideoData(bool value)
    {
        m_needVideoData = value;
    }

protected:
    virtual bool processData(const QnAbstractDataPacketPtr& packet) override
    {
        if (m_needStop)
            return true;

        nx::utils::ByteArray sendBuffer(CL_MEDIA_ALIGNMENT, 1024 * 64, AV_INPUT_BUFFER_PADDING_SIZE);

        QnAbstractMediaDataPtr media = std::dynamic_pointer_cast<QnAbstractMediaData>(packet);
        if (!media)
            return false;

        int streamIndex = media->channelNumber;
        NX_ASSERT(streamIndex <= 1);

        m_serializers[streamIndex]->setDataPacket(media);
        m_owner->sendLock();
        while(!m_needStop && m_owner->isConnected() && m_serializers[streamIndex]->getNextPacket(sendBuffer))
        {
            NX_ASSERT(sendBuffer.size() < 65536 - 4);
            quint8 header[4];
            header[0] = '$';
            header[1] = streamIndex;
            header[2] = sendBuffer.size() >> 8;
            header[3] = (quint8) sendBuffer.size();
            m_owner->sendData((const char*) &header, 4);
            m_owner->sendData(sendBuffer);
            sendBuffer.clear();
        }
        m_owner->sendUnlock();
        return true;
    }

    virtual bool needConfigureProvider() const override
    {
        return m_needVideoData;
    }

private:
    std::unique_ptr<QnRtspFfmpegEncoder> m_serializers[2]; // video + audio
    DesktopCameraConnectionProcessor* m_owner;
    bool m_needVideoData;
};

class DesktopCameraConnectionProcessor::Private: public QnTCPConnectionProcessorPrivate
{
public:
    DesktopResourcePtr desktop;
    QnAbstractStreamDataProvider* dataProvider = nullptr;
    DesktopCameraConsumer* dataConsumer = nullptr;
    nx::Mutex sendMutex;
    nx::utils::SoftwareVersion serverVersion;
};

DesktopCameraConnectionProcessor::DesktopCameraConnectionProcessor(
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    void* /*sslContext*/,
    DesktopResourcePtr desktop)
    :
    QnTCPConnectionProcessor(new Private(),
        std::move(socket),
        desktop->systemContext(),
        kMaxTcpRequestSize),
    d(dynamic_cast<Private*>(d_ptr))
{
    NX_CRITICAL(d);
    d->desktop = desktop;
    d->dataProvider = 0;
    d->dataConsumer = 0;
}

DesktopCameraConnectionProcessor::~DesktopCameraConnectionProcessor()
{
    stop();
    disconnectInternal();
}

QnResourcePtr DesktopCameraConnectionProcessor::getResource() const
{
    return resourcePool()->getResourceById(
        DesktopResource::getDesktopResourceUuid());
}

void DesktopCameraConnectionProcessor::setServerVersion(const nx::utils::SoftwareVersion& version)
{
    d->serverVersion = version;
}

nx::utils::SoftwareVersion DesktopCameraConnectionProcessor::serverVersion() const
{
    return d->serverVersion;
}

void DesktopCameraConnectionProcessor::processRequest()
{
    const auto method = d->request.requestLine.method;
    if (method == "PLAY")
    {
        NX_DEBUG(this, "Start streaming data to the VMS server");
        if (!d->dataProvider)
        {
            d->dataProvider = qnClientCoreModule->dataProviderFactory()->createDataProvider(
                d->desktop);
            d->dataConsumer = new DesktopCameraConsumer(this);
            d->dataProvider->addDataProcessor(d->dataConsumer);
            d->dataConsumer->start();
            d->dataProvider->start();
        }
        bool needVideoData = d->request.headers.find(Qn::DESKTOP_CAMERA_NO_VIDEO_HEADER_NAME) == d->request.headers.end();
        d->dataConsumer->setNeedVideoData(needVideoData);
    }
    else if (method == "TEARDOWN")
    {
        disconnectInternal();
    }
    else if (method == "KEEP-ALIVE")
    {
        // nothing to do. we restarting timer on any request
    }
    d->response.headers.insert(std::make_pair("cSeq", nx::network::http::getHeaderValue(d->request.headers, "cSeq")));
}

void DesktopCameraConnectionProcessor::disconnectInternal()
{
    if (d->dataProvider && d->dataConsumer)
        d->dataProvider->removeDataProcessor(d->dataConsumer);
    if (d->dataProvider)
        d->dataProvider->pleaseStop();
    if (d->dataConsumer)
        d->dataConsumer->pleaseStop();
    delete d->dataConsumer;
    delete d->dataProvider;

    d->dataProvider = 0;
    d->dataConsumer = 0;
}

void DesktopCameraConnectionProcessor::sendLock()
{
    d->sendMutex.lock();
}

void DesktopCameraConnectionProcessor::sendUnlock()
{
    d->sendMutex.unlock();
}

bool DesktopCameraConnectionProcessor::isConnected() const
{
    return d->socket->isConnected();
}

void DesktopCameraConnectionProcessor::sendData(const nx::utils::ByteArray& data)
{
    int sent = d->socket->send(data.constData(), data.size());
    if (sent < (int)data.size())
        d->socket->close();
}

void DesktopCameraConnectionProcessor::sendData(const char* data, int len)
{
    int sent = d->socket->send(data, len);
    if (sent < len)
        d->socket->close();
}

//--------------------------------------------------------------------------------------------------
// DesktopCameraConnection

struct DesktopCameraConnection::Private
{
    DesktopResourcePtr owner;
    QnMediaServerResourcePtr server;
    QnUuid userId;
    QnUuid moduleGuid;
    nx::network::http::Credentials credentials;
    std::shared_ptr<DesktopCameraConnectionProcessor> processor;
    std::unique_ptr<nx::network::http::HttpClient> httpClient;
    nx::Mutex mutex;
};

DesktopCameraConnection::DesktopCameraConnection(
    DesktopResourcePtr owner,
    const QnMediaServerResourcePtr& server,
    const QnUuid& userId,
    const QnUuid& moduleGuid,
    const nx::network::http::Credentials& credentials)
    :
    QnLongRunnable(),
    d(new Private())
{
    d->owner = owner;
    d->server = server;
    d->userId = userId;
    d->moduleGuid = moduleGuid;
    d->credentials = credentials;
}

DesktopCameraConnection::~DesktopCameraConnection()
{
    pleaseStop();
    stop();
}

void DesktopCameraConnection::terminatedSleep(int sleep)
{
    for (int i = 0; i < sleep/10 && !m_needStop; ++i)
        msleep(10);
}

std::unique_ptr<nx::network::AbstractStreamSocket>
DesktopCameraConnection::takeSocketFromHttpClient(
    std::unique_ptr<nx::network::http::HttpClient>& httpClient)
{
    auto socket = httpClient->takeSocket();
    if (!socket)
        return nullptr;

    auto buffer = httpClient->fetchMessageBodyBuffer();
    socket->setNonBlockingMode(false);
    auto result = std::make_unique<nx::network::BufferedStreamSocket>(
        std::move(socket), std::move(buffer));
    return result;
}

void DesktopCameraConnection::pleaseStop()
{
    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        if (d->processor)
            d->processor->pleaseStop();
        if (d->httpClient)
            d->httpClient->pleaseStop();
        d->server.reset();
    }

    base_type::pleaseStop();
}

void DesktopCameraConnection::run()
{
    const auto setupNetwork =
        [this](
            std::unique_ptr<nx::network::http::HttpClient> newClient,
            std::unique_ptr<nx::network::AbstractStreamSocket> newSocket,
            nx::utils::SoftwareVersion version)
        {
            auto newProcessor = newSocket
                ? std::make_shared<DesktopCameraConnectionProcessor>(
                    std::move(newSocket),
                    /*sslContext*/ nullptr,
                    d->owner)
                : std::shared_ptr<DesktopCameraConnectionProcessor>();
            if (newProcessor)
                newProcessor->setServerVersion(version);

            NX_MUTEX_LOCKER lock(&d->mutex);
            std::swap(d->httpClient, newClient);
            std::swap(d->processor, newProcessor);
        };

    while (!m_needStop)
    {
        QnMediaServerResourcePtr server;
        RemoteConnectionPtr connection;
        {
            NX_MUTEX_LOCKER lock(&d->mutex);
            server = d->server;
            if (server)
            {
                const auto systemContext = SystemContext::fromResource(server);
                if (NX_ASSERT(systemContext))
                    connection = systemContext->connection();
            }
        }

        if (!connection)
            break;

        const auto serverId = server->getId();
        const auto serverVersion = server->getVersion();
        auto url = server->getApiUrl();
        url.setPath("/desktop_camera");
        server.reset();

        auto newClient = std::make_unique<nx::network::http::HttpClient>(
            connection->certificateCache()->makeAdapterFunc(serverId, url));

        newClient->addAdditionalHeader("user-name", d->credentials.username);
        const auto uniqueId = DesktopResource::calculateUniqueId(
            d->moduleGuid, d->userId).toStdString();
        newClient->addAdditionalHeader("unique-id", uniqueId); //< Support for 4.2 and below.
        newClient->addAdditionalHeader("physical-id", uniqueId);
        newClient->setSendTimeout(std::chrono::milliseconds(CONNECT_TIMEOUT));
        newClient->setResponseReadTimeout(std::chrono::milliseconds(CONNECT_TIMEOUT));
        newClient->setCredentials(d->credentials);

        setupNetwork(std::move(newClient), {}, serverVersion);

        if (!d->httpClient->doGet(url))
        {
            terminatedSleep(1000 * 10);
            continue;
        }

        setupNetwork({}, takeSocketFromHttpClient(d->httpClient), serverVersion);

        QElapsedTimer timeout;
        timeout.start();
        while (!m_needStop && d->processor)
        {
            if (const auto result = d->processor->readRequest();
                result == QnTCPConnectionProcessor::ReadResult::ok)
            {
                d->processor->parseRequest();
                d->processor->processRequest();
                timeout.restart();
            }
            else if (result == QnTCPConnectionProcessor::ReadResult::error ||
                timeout.elapsed() > KEEP_ALIVE_TIMEOUT)
            {
                break;
            }
        }
    }

    setupNetwork({}, {}, {});
}

} // namespace nx::vms::client::core
