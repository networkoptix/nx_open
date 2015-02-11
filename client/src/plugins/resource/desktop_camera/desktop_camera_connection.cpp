#include "desktop_camera_connection.h"
#include "core/resource/media_server_resource.h"
#include "utils/network/tcp_connection_priv.h"
#include "api/app_server_connection.h"
#include "rtsp/rtsp_ffmpeg_encoder.h"

#include <plugins/resource/desktop_win/desktop_resource.h>


static const int CONNECT_TIMEOUT = 1000 * 5;
static const int KEEP_ALIVE_TIMEOUT = 1000 * 120;

class QnDesktopCameraDataConsumer: public QnAbstractDataConsumer
{
public:
    QnDesktopCameraDataConsumer(QnDesktopCameraConnectionProcessor* owner):
        QnAbstractDataConsumer(20), 
        m_sequence(0),
        m_owner(owner)
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
protected:
protected:

    virtual bool processData(const QnAbstractDataPacketPtr& packet) override
    {
        if (m_needStop)
            return true;

        QnByteArray sendBuffer(CL_MEDIA_ALIGNMENT, 1024 * 64);

        QnAbstractMediaDataPtr media = packet.dynamicCast<QnAbstractMediaData>();
        if (!media)
            return false;

        int streamIndex = media->channelNumber;

        m_serializers[streamIndex].setDataPacket(media);
        m_owner->sendLock();
        while(!m_needStop && m_owner->isConnected() && m_serializers[streamIndex].getNextPacket(sendBuffer))
        {
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
private:
    quint32 m_sequence;
    QnRtspFfmpegEncoder m_serializers[2]; // video + audio
    QnDesktopCameraConnectionProcessor* m_owner;
};

class QnDesktopCameraConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnDesktopResource* desktop;
    QnAbstractStreamDataProvider* dataProvider;
    QnAbstractDataConsumer* dataConsumer;
    QnMutex sendMutex;
};

QnDesktopCameraConnectionProcessor::QnDesktopCameraConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, void* sslContext, QnDesktopResource* desktop):
  QnTCPConnectionProcessor(new QnDesktopCameraConnectionProcessorPrivate(), socket)
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
        if (d->dataProvider == 0) {
            d->dataProvider = d->desktop->createDataProvider(Qn::CR_Default);
            d->dataConsumer = new QnDesktopCameraDataConsumer(this);
            d->dataProvider->addDataProcessor(d->dataConsumer);
            d->dataConsumer->start();
            d->dataProvider->start();
        }
    }
    else if (method == "TEARDOWN")
    {
        disconnectInternal();
    }
    else if (method == "KEEP-ALIVE")
    {
        // nothing to do. we restarting timer on any request
    }
    d->response.headers.insert(std::make_pair("cSeq", nx_http::getHeaderValue(d->request.headers, "cSeq")));
    //SCOPED_MUTEX_LOCK( lock, &d->sendMutex);
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
    int sended = d->socket->send(data);
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

QnDesktopCameraConnection::QnDesktopCameraConnection(QnDesktopResource* owner, const QnMediaServerResourcePtr &server):
    QnLongRunnable(),
    m_owner(owner),
    m_server(server),
    connection(0),
    processor(0)
{
    m_auth.username = QnAppServerConnectionFactory::url().userName();
    m_auth.password = QnAppServerConnectionFactory::url().password();
    m_auth.clientGuid = QnAppServerConnectionFactory::clientGuid();
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

void QnDesktopCameraConnection::pleaseStop()
{
    {
        SCOPED_MUTEX_LOCK( lock, &m_mutex);
        if (processor)
            processor->pleaseStop();
        if (connection)
            connection->getSocket()->close();
    }

    QnLongRunnable::pleaseStop();
}

void QnDesktopCameraConnection::run()
{
    while (!m_needStop)
    {
        {
            SCOPED_MUTEX_LOCK( lock, &m_mutex);
            delete processor;
            processor = 0;
            delete connection;
            connection = 0;

            QAuthenticator auth;
            auth.setUser(m_auth.username);
            auth.setPassword(m_auth.password);
            connection = new CLSimpleHTTPClient(m_server->getApiUrl(), CONNECT_TIMEOUT, auth);
            connection->addHeader("user-name", auth.user().toUtf8());
            connection->addHeader("user-id", m_auth.clientGuid.toUtf8());
        }

        CLHttpStatus status = connection->doGET(QByteArray("desktop_camera"));
        if (status != CL_HTTP_SUCCESS) {
            terminatedSleep(1000 * 10);
            continue;
        }

        processor = new QnDesktopCameraConnectionProcessor(connection->getSocket(), 0, m_owner);

        QTime timeout;
        timeout.restart();
        while (!m_needStop && connection->getSocket()->isConnected())
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

    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    delete processor;
    delete connection;

    processor = 0;
    connection = 0;
}
