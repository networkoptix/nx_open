#include "proxy_audio_receiver.h"
#include "proxy_audio_transmitter.h"

#include "network/universal_request_processor_p.h"
#include "network/tcp_connection_priv.h"
#include "network/universal_tcp_listener.h"
#include <streaming/audio_streamer_pool.h>
#include <rest/handlers/audio_transmission_rest_handler.h>
#include <nx/streaming/nx_rtp_parser.h>

namespace
{
    const QString kClientIdParamName("clientId");
    const QString kResourceIdParamName("resourceId");
    const QString kActionParamName("action");
    const QString kStartStreamAction("start");
    const QString kStopStreamAction("stop");

    bool readBytes(QSharedPointer<AbstractStreamSocket> socket, quint8* dst, int size)
    {
        while (size > 0)
        {
            int readed = socket->recv(dst, size);
            if (readed < 1)
                return false;
            dst += readed;
            size -= readed;
        }
        return true;
    }

    bool readHttpHeaders(QSharedPointer<AbstractStreamSocket> socket)
    {
        int toRead = QnProxyAudioTransmitter::kFixedPostRequest.size();
        QByteArray buffer;
        buffer.resize(toRead);
        return readBytes(socket, (quint8*) buffer.data(), toRead);
    }
}

class QnProxyDesktopDataProvider:
    public QnAbstractStreamDataProvider
{
private:
    quint8* m_recvBuffer;
    QnNxRtpParser m_parser;

public:
    QnProxyDesktopDataProvider(const QnUuid& cameraId):
        QnAbstractStreamDataProvider(QnResourcePtr()),
        m_parser(cameraId.toString()),
        m_cameraId(cameraId)
    {
        m_recvBuffer = new quint8[65536];
    }

    virtual ~QnProxyDesktopDataProvider()
    {
        stop();
        delete [] m_recvBuffer;
    }

    void setSocket(const QSharedPointer<AbstractStreamSocket>& socket)
    {
        stop();
        QnMutexLocker lock(&m_mutex);
        m_socket = socket;
    }

protected:

    QnAbstractMediaDataPtr getNextData()
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_needStop && m_socket->isConnected())
        {
            if (!readBytes(m_socket, m_recvBuffer, 4))
            {
                m_socket->close();
                break;
            }

            int packetSize = (m_recvBuffer[2] << 8) + m_recvBuffer[3];
            if (!readBytes(m_socket, m_recvBuffer, packetSize))
            {
                m_socket->close();
                break;
            }

            bool gotData;
            m_parser.processData(m_recvBuffer, 0, packetSize, QnRtspStatistic(), gotData);
            if (gotData)
                return m_parser.nextData();
        }
        return QnAbstractMediaDataPtr();
    }

    virtual void run() override
    {
        while(!needToStop())
        {
            QnAbstractMediaDataPtr data = getNextData();
            if (!data)
                break;
            data->dataProvider = this;
            if (dataCanBeAccepted())
                putData(std::move(data));
        }
        m_socket->close();

        QnAbstractMediaDataPtr eofData(new QnEmptyMediaData());
        eofData->dataProvider = this;
        putData(eofData);
    }

private:
    QSharedPointer<AbstractStreamSocket> m_socket;
    mutable QnMutex m_mutex;
    QnUuid m_cameraId;
};

typedef QSharedPointer<QnProxyDesktopDataProvider> QnProxyDesktopDataProviderPtr;


QnAudioProxyReceiver::QnAudioProxyReceiver(
    QSharedPointer<AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
:
    QnTCPConnectionProcessor(socket, owner)
{
    setObjectName(::toString(this));
}

void QnAudioProxyReceiver::run()
{
    Q_D(QnTCPConnectionProcessor);

    parseRequest();

    auto queryItems = QUrlQuery(d->request.requestLine.url.query()).queryItems();

    QnRequestParams params;
    for(auto itr = queryItems.begin(); itr != queryItems.end(); ++itr)
        params.insertMulti(itr->first, itr->second);

    QString errorStr;
    if (!QnAudioTransmissionRestHandler::validateParams(params, errorStr))
    {
        sendResponse(nx::network::http::StatusCode::badRequest, QByteArray());
        return;
    }
    sendResponse(nx::network::http::StatusCode::ok, QByteArray());

    const QnUuid clientId(params[kClientIdParamName]);

    auto resourceId = params[kResourceIdParamName];
    QnAudioStreamerPool::Action action = (params[kActionParamName] == kStartStreamAction)
        ? QnAudioStreamerPool::Action::Start
        : QnAudioStreamerPool::Action::Stop;

    // process 2-nd POST request with unlimited length

    if (!readHttpHeaders(d->socket))
        return;

    QnProxyDesktopDataProviderPtr desktopDataProvider(new QnProxyDesktopDataProvider(QnUuid::fromStringSafe(resourceId)));
    desktopDataProvider->setSocket(d->socket);

    QString errString;
    if (!QnAudioStreamerPool::instance()->startStopStreamToResource(desktopDataProvider, QnUuid(resourceId), action, errString))
    {
        qWarning() << "Cant start audio uploading to camera" << resourceId << errString;
    }
    else
    {
        takeSocket();
    }
}
