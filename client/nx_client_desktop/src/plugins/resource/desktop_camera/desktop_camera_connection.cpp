#include "desktop_camera_connection.h"

#include <QtCore/QElapsedTimer>

#include <common/common_module.h>

#include "core/resource/media_server_resource.h"
#include "network/tcp_connection_priv.h"
#include "api/app_server_connection.h"
#include "rtsp/rtsp_ffmpeg_encoder.h"

#include <plugins/resource/desktop_camera/desktop_resource_base.h>
#include <nx/network/http/custom_headers.h>
#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/streaming/config.h>
#include <nx/network/buffered_stream_socket.h>


static const int CONNECT_TIMEOUT = 1000 * 5;
static const int KEEP_ALIVE_TIMEOUT = 1000 * 120;

class QnDesktopCameraDataConsumer: public QnAbstractDataConsumer
{
public:
    QnDesktopCameraDataConsumer(QnDesktopCameraConnectionProcessor* owner):
        QnAbstractDataConsumer(50),
        m_sequence(0),
        m_owner(owner),
        m_needVideoData(false)
    {
        for (int i = 0; i < 2; ++i) {
            m_serializers[i].setAdditionFlags(0);
            m_serializers[i].setLiveMarker(true);
        }
    }

    virtual ~QnDesktopCameraDataConsumer()
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

        QnByteArray sendBuffer(CL_MEDIA_ALIGNMENT, 1024 * 64);

        QnAbstractMediaDataPtr media = std::dynamic_pointer_cast<QnAbstractMediaData>(packet);
        if (!media)
            return false;

        int streamIndex = media->channelNumber;
        NX_ASSERT(streamIndex <= 1);

        m_serializers[streamIndex].setDataPacket(media);
        m_owner->sendLock();
        while(!m_needStop && m_owner->isConnected() && m_serializers[streamIndex].getNextPacket(sendBuffer))
        {
            NX_ASSERT(sendBuffer.size() < 65536 - 4);
            quint8 header[4];
            header[0] = '$';
            header[1] = streamIndex;
            header[2] = sendBuffer.size() >> 8;
            header[3] = (quint8) sendBuffer.size();
            m_owner->sendData((const char*) &header, 4);

            static AVRational r = {1, 1000000};
            AVRational time_base = {1, (int) m_serializers[streamIndex].getFrequency() };
            qint64 packetTime = av_rescale_q(packet->timestamp, r, time_base);

            QnRtspEncoder::buildRTPHeader(sendBuffer.data(), m_serializers[streamIndex].getSSRC(), m_serializers[streamIndex].getRtpMarker(),
                           packetTime, m_serializers[streamIndex].getPayloadtype(), m_sequence++);
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
    quint32 m_sequence;
    QnRtspFfmpegEncoder m_serializers[2]; // video + audio
    QnDesktopCameraConnectionProcessor* m_owner;
    bool m_needVideoData;
};

class QnDesktopCameraConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnDesktopResource* desktop;
    QnAbstractStreamDataProvider* dataProvider;
    QnDesktopCameraDataConsumer* dataConsumer;
    QnMutex sendMutex;
};

QnDesktopCameraConnectionProcessor::QnDesktopCameraConnectionProcessor(
    QSharedPointer<nx::network::AbstractStreamSocket> socket,
    void* sslContext,
    QnDesktopResource* desktop)
    :
    QnTCPConnectionProcessor(new QnDesktopCameraConnectionProcessorPrivate(),
        socket,
        desktop->commonModule())
{
    Q_UNUSED(sslContext)
    Q_D(QnDesktopCameraConnectionProcessor);
    d->desktop = desktop;
    d->dataProvider = 0;
    d->dataConsumer = 0;
}

QnDesktopCameraConnectionProcessor::~QnDesktopCameraConnectionProcessor()
{
    Q_D(QnDesktopCameraConnectionProcessor);

    stop();
    disconnectInternal();

    d->socket.clear(); // we have not ownership for socket in this class
}

void QnDesktopCameraConnectionProcessor::processRequest()
{
    Q_D(QnDesktopCameraConnectionProcessor);
    QByteArray method = d->request.requestLine.method;
    if (method == "PLAY")
    {
        if (d->dataProvider == 0)
        {
            d->dataProvider = d->desktop->createDataProvider(Qn::CR_Default);
            d->dataConsumer = new QnDesktopCameraDataConsumer(this);
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
    //QnMutexLocker lock( &d->sendMutex );
    //sendResponse("RTSP", CODE_OK, QByteArray(), QByteArray());
}

void QnDesktopCameraConnectionProcessor::disconnectInternal()
{
    Q_D(QnDesktopCameraConnectionProcessor);

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

void QnDesktopCameraConnectionProcessor::sendLock()
{
    Q_D(QnDesktopCameraConnectionProcessor);
    d->sendMutex.lock();
}

void QnDesktopCameraConnectionProcessor::sendUnlock()
{
    Q_D(QnDesktopCameraConnectionProcessor);
    d->sendMutex.unlock();
}

bool QnDesktopCameraConnectionProcessor::isConnected() const
{
    Q_D(const QnDesktopCameraConnectionProcessor);
    return d->socket->isConnected();
}


void QnDesktopCameraConnectionProcessor::sendData(const QnByteArray& data)
{
    Q_D(QnDesktopCameraConnectionProcessor);
    int sended = d->socket->send(data.constData(), data.size());
    if (sended < (int)data.size())
        d->socket->close();
}

void QnDesktopCameraConnectionProcessor::sendData(const char* data, int len)
{
    Q_D(QnDesktopCameraConnectionProcessor);
    int sended = d->socket->send(data, len);
    if (sended < len)
        d->socket->close();
}

// --------------- QnDesktopCameraconnection ------------------

QnDesktopCameraConnection::QnDesktopCameraConnection(
    QnDesktopResource* owner,
    const QnMediaServerResourcePtr& server,
    const QnUuid& userId)
    :
    QnLongRunnable(),
    m_owner(owner),
    m_server(server),
    m_userId(userId)
{
}

QnDesktopCameraConnection::~QnDesktopCameraConnection()
{
    pleaseStop();
    stop();
}

void QnDesktopCameraConnection::terminatedSleep(int sleep)
{
    for (int i = 0; i < sleep/10 && !m_needStop; ++i)
        msleep(10);
}

QSharedPointer<nx::network::AbstractStreamSocket> QnDesktopCameraConnection::takeSocketFromHttpClient(
    std::unique_ptr<nx::network::http::HttpClient>& httpClient)
{
    auto socket = QSharedPointer<nx::network::BufferedStreamSocket>(
        new nx::network::BufferedStreamSocket(httpClient->takeSocket()));

    auto buffer = httpClient->fetchMessageBodyBuffer();

    if (buffer.size())
        socket->injectRecvData(std::move(buffer));

    return socket;
}

void QnDesktopCameraConnection::pleaseStop()
{
    {
        QnMutexLocker lock( &m_mutex );
        if (processor)
            processor->pleaseStop();
        if (httpClient)
            httpClient->pleaseStop();
        if (tcpSocket)
            tcpSocket->shutdown();
    }

    base_type::pleaseStop();
}

void QnDesktopCameraConnection::run()
{
    const auto setupNetwork =
        [this](
            std::unique_ptr<nx::network::http::HttpClient> newClient,
            QSharedPointer<AbstractStreamSocket> newSocket)
        {
            auto newProcessor = newSocket
                ? std::make_shared<QnDesktopCameraConnectionProcessor>(newSocket, nullptr, m_owner)
                : std::shared_ptr<QnDesktopCameraConnectionProcessor>();

            QnMutexLocker lock(&m_mutex);
            std::swap(tcpSocket, newSocket);
            std::swap(httpClient, newClient);
            std::swap(processor, newProcessor);
        };

    while (!m_needStop)
    {
        QAuthenticator auth;
        auth.setUser(commonModule()->currentUrl().userName());
        auth.setPassword(commonModule()->currentUrl().password());
        setupNetwork(std::make_unique<nx::network::http::HttpClient>(), QSharedPointer<AbstractStreamSocket>());

        httpClient->addAdditionalHeader("user-name", auth.user().toUtf8());
        // TODO: #GDM #3.2 Remove 3.1 compatibility layer.
        httpClient->addAdditionalHeader("user-id", commonModule()->moduleGUID().toByteArray());
        httpClient->addAdditionalHeader("unique-id",
            QnDesktopResource::calculateUniqueId(commonModule()->moduleGUID(), m_userId).toUtf8());
        httpClient->setSendTimeoutMs(CONNECT_TIMEOUT);
        httpClient->setResponseReadTimeoutMs(CONNECT_TIMEOUT);

        httpClient->setUserName(auth.user());
        httpClient->setUserPassword(auth.password());

        if (auth.user().isEmpty() || auth.password().isEmpty()) {
            terminatedSleep(1000 * 10);
            continue;
        }

        nx::utils::Url url(m_server->getApiUrl());
        url.setScheme(lit("http"));
        url.setPath(lit("/desktop_camera"));

        if (!httpClient->doGet(url))
        {
            terminatedSleep(1000 * 10);
            continue;
        }

        setupNetwork(nullptr, takeSocketFromHttpClient(httpClient));

        QElapsedTimer timeout;
        timeout.start();
        while (!m_needStop && tcpSocket->isConnected())
        {
            if (processor->readRequest()) {
                processor->parseRequest();
                processor->processRequest();
                timeout.restart();
            }
            else if (timeout.elapsed() > KEEP_ALIVE_TIMEOUT)
            {
                break;
            }
        }
    }

    setupNetwork(nullptr, QSharedPointer<AbstractStreamSocket>());
}
