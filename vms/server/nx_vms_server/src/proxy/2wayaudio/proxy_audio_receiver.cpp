#include "proxy_audio_receiver.h"
#include "proxy_audio_transmitter.h"

#include "network/universal_request_processor_p.h"
#include "network/tcp_connection_priv.h"
#include "network/universal_tcp_listener.h"
#include <streaming/audio_streamer_pool.h>
#include <rest/handlers/audio_transmission_rest_handler.h>
#include <nx/streaming/rtp/parsers/nx_rtp_parser.h>
#include <media_server/media_server_module.h>
#include <nx/network/buffered_stream_socket.h>

namespace
{
    const QString kClientIdParamName("clientId");
    const QString kResourceIdParamName("resourceId");
    const QString kActionParamName("action");
    const QString kStartStreamAction("start");
    const QString kStopStreamAction("stop");

    bool readBytes(nx::network::AbstractStreamSocket* socket, quint8* dst, int size)
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

    bool readHttpHeaders(nx::network::AbstractStreamSocket* socket, QByteArray* outPayloadBuffer)
    {
        char headersBuffer[1024];
        char* dst = headersBuffer;
        int bytesRead = 0;
        while (bytesRead < sizeof(headersBuffer))
        {
            int result = socket->recv(&headersBuffer[bytesRead], sizeof(headersBuffer) - bytesRead);
            if (result < 1)
                return false;
            bytesRead += result;
            static const QByteArray kDelimiter("\r\n\r\n");
            int delimiterPos = QByteArray::fromRawData(headersBuffer, bytesRead).indexOf(kDelimiter);
            if (delimiterPos > 0)
            {
                delimiterPos += kDelimiter.length();
                int bytesLeft = bytesRead - delimiterPos;
                outPayloadBuffer->resize(bytesLeft);
                if (bytesLeft > 0)
                    memcpy(outPayloadBuffer->data(), headersBuffer + delimiterPos, bytesLeft);
                return true;
            }
        }
        return false;
    }
}

class QnProxyDesktopDataProvider:
    public QnAbstractStreamDataProvider
{
private:
    quint8* m_recvBuffer;
    nx::streaming::rtp::QnNxRtpParser m_parser;

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

    void setSocket(std::unique_ptr<nx::network::AbstractStreamSocket> socket)
    {
        stop();
        QnMutexLocker lock(&m_mutex);
        m_socket = std::move(socket);
    }

protected:

    QnAbstractMediaDataPtr getNextData()
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_needStop && m_socket->isConnected())
        {
            if (!readBytes(m_socket.get(), m_recvBuffer, 4))
            {
                m_socket->close();
                break;
            }

            int packetSize = (m_recvBuffer[2] << 8) + m_recvBuffer[3];
            if (!readBytes(m_socket.get(), m_recvBuffer, packetSize))
            {
                m_socket->close();
                break;
            }

            bool gotData;
            m_parser.processData(m_recvBuffer, 0, packetSize, gotData);
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
    std::unique_ptr<nx::network::AbstractStreamSocket> m_socket;
    mutable QnMutex m_mutex;
    QnUuid m_cameraId;
};

typedef QSharedPointer<QnProxyDesktopDataProvider> QnProxyDesktopDataProviderPtr;

QnAudioProxyReceiver::QnAudioProxyReceiver(
    QnMediaServerModule* serverModule,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
    :
    QnTCPConnectionProcessor(std::move(socket), owner),
    m_serverModule(serverModule)
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

    auto resourceId = params[kResourceIdParamName];
    QnAudioStreamerPool::Action action = (params[kActionParamName] == kStartStreamAction)
        ? QnAudioStreamerPool::Action::Start
        : QnAudioStreamerPool::Action::Stop;

    // process 2-nd POST request with unlimited length
    QByteArray payloadData;
    if (!readHttpHeaders(d->socket.get(), &payloadData))
        return;

    auto bufferedSocket = std::make_unique<nx::network::BufferedStreamSocket>(takeSocket(), payloadData);

    QnProxyDesktopDataProviderPtr desktopDataProvider(new QnProxyDesktopDataProvider(QnUuid::fromStringSafe(resourceId)));
    desktopDataProvider->setSocket(std::move(bufferedSocket));

    QString errString;
    if (!m_serverModule->audioStreamPool()->startStopStreamToResource(desktopDataProvider, QnUuid(resourceId), action, errString))
    {
        qWarning() << "Cant start audio uploading to camera" << resourceId << errString;
    }
}
