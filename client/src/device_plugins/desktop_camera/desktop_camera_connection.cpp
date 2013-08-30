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
            m_owner->sendData(m_sendBuffer);
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
        d->dataProvider->removeDataProcessor(d->dataConsumer);
        delete d->dataConsumer;
        delete d->dataProvider;
        d->dataProvider = 0;
    }

    //sendResponse("HTTP", CODE_OK, QByteArray(), QByteArray());
}

void QnDesktopCameraConnectionProcessor::sendData(const QnByteArray& data)
{
    Q_D(QnDesktopCameraConnectionProcessor);
    d->socket->send(data);
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
        delete connection;

        QAuthenticator auth;
        auth.setUser(QnAppServerConnectionFactory::defaultUrl().userName());
        auth.setPassword(QnAppServerConnectionFactory::defaultUrl().password());
        connection = new CLSimpleHTTPClient(m_mServer->getApiUrl(), CONNECT_TIMEOUT, auth);
        connection->addHeader("user-name", auth.user().toUtf8());

        CLHttpStatus status = connection->doGET("desktopCamera");
        if (status != CL_HTTP_SUCCESS) {
            terminatedSleep(1000 * 10);
            continue;
        }

        QByteArray data;
        connection->readAll(data); // server answer

        processor = new QnDesktopCameraConnectionProcessor(connection->getSocket().data(), 0, m_owner);

        QTime timeout;
        timeout.restart();
        while (m_needStop && connection->getSocket()->isConnected())
        {
            if (processor->readRequest()) {
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
