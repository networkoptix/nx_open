#include "2wayaudio_proxy_receiver.h"
#include "network/universal_request_processor_p.h"
#include "network/tcp_connection_priv.h"
#include "network/universal_tcp_listener.h"
#include <rest/handlers/audio_transmission_rest_handler.h>
#include <2wayaudio/proxy_audio_transmitter.h>
#include <network/ffmpeg_rtp_parser.h>
#include <streaming/audio_streamer_pool.h>
#include <utils/common/from_this_to_shared.h>

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

    static QMutex m_mutex;
    static QMap<QString, QSharedPointer<QnProxyDesktopDataProvider>> m_proxyProviders;
}

struct QnTwoWayAudioProxyReceiverConnectionPrivate: public QnTCPConnectionProcessorPrivate
{
    QnTwoWayAudioProxyReceiverConnectionPrivate():
        QnTCPConnectionProcessorPrivate(),
        takeSocketOwnership(false)
    {
    }

    bool takeSocketOwnership;
};

class QnProxyDesktopDataProvider:
    public QnAbstractStreamDataProvider,
    public QnFromThisToShared<QnProxyDesktopDataProvider>
{
private:
    quint8* m_recvBuffer;
    QnFfmpegRtpParser m_parser;

public:
    QnProxyDesktopDataProvider(const QnUuid& cameraId):
        QnAbstractStreamDataProvider(QnResourcePtr()),
        QnFromThisToShared<QnProxyDesktopDataProvider>(),
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
        QMutexLocker lock(&m_mutex);
        m_socket = socket;
    }

protected:

    QnAbstractMediaDataPtr getNextData()
    {
        QMutexLocker lock(&m_mutex);
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
            m_parser.processData(m_recvBuffer, 0, packetSize, RtspStatistic(), gotData);
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
        QString errString;
        QnAudioStreamerPool::instance()->startStopStreamToResource(toSharedPointer(), m_cameraId, QnAudioStreamerPool::Action::Stop, errString);
    }

private:
    QSharedPointer<AbstractStreamSocket> m_socket;
    mutable QMutex m_mutex;
    QnUuid m_cameraId;
};

QnTwoWayAudioProxyReceiverConnection::QnTwoWayAudioProxyReceiverConnection(QSharedPointer<AbstractStreamSocket> socket,
                                                     QnHttpConnectionListener* owner):
    QnTCPConnectionProcessor(new QnTwoWayAudioProxyReceiverConnectionPrivate, socket)
{
    Q_D(QnTwoWayAudioProxyReceiverConnection);
    setObjectName( lit("QnTwoWayAudioProxyReceiverConnection") );
}

void QnTwoWayAudioProxyReceiverConnection::run()
{
    Q_D(QnTwoWayAudioProxyReceiverConnection);

    parseRequest();

    auto queryItems = QUrlQuery(d->request.requestLine.url.query()).queryItems();

    QnRequestParams params;
    for(auto itr = queryItems.begin(); itr != queryItems.end(); ++itr)
        params.insertMulti(itr->first, itr->second);

    QString errorStr;
    if (!QnAudioTransmissionRestHandler::validateParams(params, errorStr))
    {
        sendResponse(nx_http::StatusCode::badRequest, QByteArray());
        return;
    }
    sendResponse(nx_http::StatusCode::ok, QByteArray());

    const QnUuid clientId = params[kClientIdParamName];

    auto resourceId = params[kResourceIdParamName];
    QnAudioStreamerPool::Action action = (params[kActionParamName] == kStartStreamAction)
        ? QnAudioStreamerPool::Action::Start
        : QnAudioStreamerPool::Action::Stop;

    // process 2-nd POST request with unlimited length

    if (!readHttpHeaders(d->socket))
        return;

    //QSharedPointer<QnProxyDesktopDataProvider> desktopDataProvider(new QnProxyDesktopDataProvider(tcpSocket));
    QSharedPointer<QnProxyDesktopDataProvider> desktopDataProvider;
    {
        QMutexLocker lock(&m_mutex);
        QString key = clientId.toString() + resourceId;
        auto itr = m_proxyProviders.find(key);
        if (itr == m_proxyProviders.end())
            itr = m_proxyProviders.insert(key, QSharedPointer<QnProxyDesktopDataProvider>(new QnProxyDesktopDataProvider(resourceId)));
        desktopDataProvider = itr.value();
        desktopDataProvider->setSocket(d->socket);
    }

    QString errString;
    if (!QnAudioStreamerPool::instance()->startStopStreamToResource(desktopDataProvider, resourceId, action, errString))
    {
        qWarning() << "Cant start audio uploading to camera" << resourceId << errString;
    }
    else
    {
        d->socket.clear();
        d->takeSocketOwnership = true;
    }
}

bool QnTwoWayAudioProxyReceiverConnection::isTakeSockOwnership() const
{
    Q_D(const QnTwoWayAudioProxyReceiverConnection);
    return d->takeSocketOwnership;
}
