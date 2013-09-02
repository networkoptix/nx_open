#include "desktop_camera_connection.h"
#include "core/resource/media_server_resource.h"
#include "utils/network/tcp_connection_priv.h"
#include "api/app_server_connection.h"
#include "../desktop_win/device/desktop_resource.h"
#include "rtsp/rtsp_ffmpeg_encoder.h"


static const int CONNECT_TIMEOUT = 1000 * 5;
static const int KEEP_ALIVE_TIMEOUT = 1000 * 60;

class QnDesktopCameraDataConsumer: public QnAbstractDataConsumer
{
public:
    QnDesktopCameraDataConsumer(QnDesktopCameraConnectionProcessor* owner):
        QnAbstractDataConsumer(20), 
        m_sendBuffer(CL_MEDIA_ALIGNMENT, 1024*64),
        m_owner(owner)
    {
        m_serializer.setAdditionFlags(0);
        m_serializer.setLiveMarker(true);
    }
    ~QnDesktopCameraDataConsumer()
    {
        stop();
    }
protected:
protected:

    virtual bool processData(QnAbstractDataPacketPtr packet) override
    {
        m_serializer.setDataPacket(packet.dynamicCast<QnAbstractMediaData>());
        while(!m_needStop && m_serializer.getNextPacket(m_sendBuffer))
        {
            quint8 header[4];
            header[0] = '$';
            header[1] = 0;
            header[2] = m_sendBuffer.size() >> 8;
            header[3] = (quint8) m_sendBuffer.size();
            m_owner->sendData((const char*) &header, 4);
            m_owner->sendData(m_sendBuffer);
        }
        return true;
    }
private:
    QnRtspFfmpegEncoder m_serializer;
    QnByteArray m_sendBuffer;
    QnDesktopCameraConnectionProcessor* m_owner;
};

class QnDesktopCameraConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnDesktopResource* desktop;
    QnAbstractStreamDataProvider* dataProvider;
    QnAbstractDataConsumer* dataConsumer;
};

QnDesktopCameraConnectionProcessor::QnDesktopCameraConnectionProcessor(TCPSocket* socket, void* sslContext, QnDesktopResource* desktop):
  QnTCPConnectionProcessor(new QnDesktopCameraConnectionProcessorPrivate(), socket, sslContext)
{
    Q_D(QnDesktopCameraConnectionProcessor);
    d->desktop = desktop;
    d->dataProvider = 0;
    d->dataConsumer = 0;
}

QnDesktopCameraConnectionProcessor::~QnDesktopCameraConnectionProcessor()
{
    Q_D(QnDesktopCameraConnectionProcessor);


    disconnectInternal();

    d->socket = 0; // we have not ownership for socket in this class
}

void QnDesktopCameraConnectionProcessor::processRequest()
{
    Q_D(QnDesktopCameraConnectionProcessor);
    QString method = d->requestHeaders.method();
    if (method == lit("CONNECT"))
    {
        if (d->dataProvider == 0) {
            d->dataProvider = d->desktop->createDataProvider(QnResource::Role_Default);
            d->dataConsumer = new QnDesktopCameraDataConsumer(this);
            d->dataProvider->addDataProcessor(d->dataConsumer);
            d->dataConsumer->start();
            d->dataProvider->start();
        }
    }
    else if (method == lit("DISCONNECT"))
    {
        disconnectInternal();
    }

    //sendResponse("HTTP", CODE_OK, QByteArray(), QByteArray());
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
    d->dataProvider = 0;
}

void QnDesktopCameraConnectionProcessor::sendData(const QnByteArray& data)
{
    Q_D(QnDesktopCameraConnectionProcessor);
    d->socket->send(data);
}

void QnDesktopCameraConnectionProcessor::sendData(const char* data, int len)
{
    Q_D(QnDesktopCameraConnectionProcessor);
    d->socket->send(data, len);
}

// --------------- QnDesktopCameraconnection ------------------

QnDesktopCameraConnection::QnDesktopCameraConnection(QnDesktopResource* owner, QnMediaServerResourcePtr mServer):
    QnLongRunnable(),
    m_owner(owner),
    m_mServer(mServer)
{

}

QnDesktopCameraConnection::~QnDesktopCameraConnection()
{
    stop();
}

void QnDesktopCameraConnection::terminatedSleep(int sleep)
{
    for (int i = 0; i < sleep/10 && !m_needStop; ++i)
        msleep(10);
}

void QnDesktopCameraConnection::run()
{
    CLSimpleHTTPClient* connection = 0;
    QnDesktopCameraConnectionProcessor* processor = 0;

    while (!m_needStop)
    {
        delete processor;
        processor = 0;
        delete connection;
        connection = 0;

        QAuthenticator auth;
        auth.setUser(QnAppServerConnectionFactory::defaultUrl().userName());
        auth.setPassword(QnAppServerConnectionFactory::defaultUrl().password());
        connection = new CLSimpleHTTPClient(m_mServer->getApiUrl(), CONNECT_TIMEOUT, auth);
        connection->addHeader("user-name", auth.user().toUtf8());

        CLHttpStatus status = connection->doGET("desktop_camera");
        if (status != CL_HTTP_SUCCESS) {
            terminatedSleep(1000 * 10);
            continue;
        }

        processor = new QnDesktopCameraConnectionProcessor(connection->getSocket().data(), 0, m_owner);

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

    delete processor;
    delete connection;
}
