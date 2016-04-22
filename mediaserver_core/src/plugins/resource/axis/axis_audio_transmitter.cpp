#include "axis_audio_transmitter.h"
#include <utils/common/sleep.h>
#include <utils/network/http/httptypes.h>
#include <utils/network/simple_http_client.h>

namespace
{
    const QString kAxisAudioTransmitUrl("/axis-cgi/audio/transmit.cgi");
    const QSet<CodecID> kSupportedCodecs = {CodecID::CODEC_ID_PCM_MULAW};
}

QSet<CodecID> QnAxisAudioTransmitter::getSupportedAudioCodecs()
{
    return kSupportedCodecs;
}

QnAxisAudioTransmitter::QnAxisAudioTransmitter(QnSecurityCamResource* res, CodecID codecId) :
    QnAbstractAudioTransmitter(),
    m_resource(res),
    m_codecId(codecId),
    m_transcoder(codecId),
    m_audioContextOpened(false),
    m_connectionEstablished(false),
    m_canProcessData(false)
{
    initSocket();
}

QnAxisAudioTransmitter::~QnAxisAudioTransmitter()
{
    qDebug() << "Destroying Axis audio transmitter";
    pleaseStop();
}

bool QnAxisAudioTransmitter::initSocket()
{
    m_socket = std::make_shared<TCPSocket>();
    return !!m_socket;
}

QString QnAxisAudioTransmitter::getAudioMimeType(CodecID codecId) const
{
    if (codecId == CodecID::CODEC_ID_PCM_MULAW)
        return lit("audio/basic");
    else if (codecId == CodecID::CODEC_ID_ADPCM_G726)
        return lit("audio/G726-32");
    else if (codecId == CodecID::CODEC_ID_AAC)
        return lit("audio/mpeg4-generic");
    else
        return lit("audio/basic");
}

SocketAddress QnAxisAudioTransmitter::getSocketAddress() const
{
    QUrl url = m_resource->getUrl();

    auto host = url.host();
    if (host.isEmpty())
        host = m_resource->getHostAddress();

    auto port = url.port(nx_http::DEFAULT_HTTP_PORT);
    if (!port)
        port = nx_http::DEFAULT_HTTP_PORT;

    return SocketAddress(host, port);
}

QByteArray QnAxisAudioTransmitter::buildTransmitRequest() const
{
    auto  auth = m_resource->getAuth();
    QByteArray transmitRequest;
    SocketAddress sockAddr = getSocketAddress();

    nx_http::BufferType authHeaderBuf;
    nx_http::header::BasicAuthorization authHeader(
        nx_http::StringType(auth.user().toLatin1()),
        nx_http::StringType(auth.password().toLatin1()));
    authHeader.serialize(&authHeaderBuf);

    transmitRequest
        .append(lit("POST %1 HTTP/1.1\r\n")
            .arg(kAxisAudioTransmitUrl))
        .append(lit("Host: %1\r\n")
            .arg(sockAddr.toString()))
        .append(lit("Content-Type: %1\r\n")
            .arg(getAudioMimeType(m_codecId)))
        .append(lit("Content-Length: 99999999\r\n"))
        .append(lit("Connection: Keep-Alive\r\n"))
        .append(lit("Cache-Control: no-cache\r\n"))
        .append(lit("Authorization: %1\r\n")
            .arg(QString::fromLatin1(authHeaderBuf)))
        .append(lit("\r\n"));

    return transmitRequest;
}

bool QnAxisAudioTransmitter::establishConnection()
{
    const auto sockAddr = getSocketAddress();

    qDebug() << "Axis audio transmitter, Connecting to " << sockAddr.toString();

    m_socket->reopen();
    auto isConnected = m_socket->connect(sockAddr, 3000);
    if (!isConnected)
        return false;

    auto transmitRequest = buildTransmitRequest();

    qDebug() << "Axis audio transmitter, sending post request" << QString(transmitRequest);

    int totalBytesSent(0);
    auto data = transmitRequest.constData();
    auto dataSize = transmitRequest.size();

    while (totalBytesSent < transmitRequest.size())
    {
        auto bytesSent = m_socket->send(data + totalBytesSent, dataSize - totalBytesSent);
        if (bytesSent == -1)
            break;
        else
            totalBytesSent += bytesSent;
    }

    m_connectionEstablished = totalBytesSent == transmitRequest.size();

    if (!m_connectionEstablished)
        qDebug() << "Axis audio transmitter, error occured during connection establishing: "
                << SystemError::getLastOSErrorText();

    return m_connectionEstablished;
}

bool QnAxisAudioTransmitter::processAudioData(QnConstAbstractMediaDataPtr &data)
{
    {
        QnMutexLocker lk(&m_connectionMutex);
        if (!m_connectionEstablished)
            establishConnection();

        if (!m_connectionEstablished)
            return false;
    }

    QnAbstractMediaDataPtr transcoded;

    if (!m_audioContextOpened)
    {
        m_transcoder.open(data->context);
        m_audioContextOpened = true;
    }

    bool inputDataDepleted = false;
    do {

        m_transcoder.transcodePacket(inputDataDepleted ? QnConstAbstractMediaDataPtr() : data, &transcoded);

        if (transcoded && transcoded->dataSize() > 0)
        {
            auto data = transcoded->data();
            auto dataSize = transcoded->dataSize();
            int totalBytesSent = 0;

            while (totalBytesSent < dataSize)
            {
                auto bytesSent = m_socket->send(data + totalBytesSent, dataSize - totalBytesSent);
                if (bytesSent == -1)
                {
                    qDebug() << "Axis audio transmitter, error occured during audio data transmission:"
                            << SystemError::getLastOSErrorText() << ", "
                            << "Error code: "
                            << SystemError::getLastOSErrorCode();

                    if (establishConnection())
                        continue;
                    else
                        break;
                }
                else
                {
                    totalBytesSent += bytesSent;
                }
            }

            if (totalBytesSent < dataSize)
                qDebug() << "Axis audio transmitter, error occured during audio data transmission:"
                        << SystemError::getLastOSErrorText() << ", "
                        << "Error code: "
                        << SystemError::getLastOSErrorCode();

        }

        inputDataDepleted = true;
    } while (transcoded);

    return true;
}


