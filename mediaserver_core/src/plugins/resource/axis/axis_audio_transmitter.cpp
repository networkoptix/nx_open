
#include "axis_audio_transmitter.h"

#include <nx/network/http/httptypes.h>
#include <nx/network/simple_http_client.h>
#include <utils/common/sleep.h>


namespace
{
    static const QString kAxisAudioTransmitUrl("/axis-cgi/audio/transmit.cgi");
    static const int kConnectionTimeoutMs = 3000;
}

bool QnAxisAudioTransmitter::isCompatible(const QnOutputAudioFormat& format) const
{
    return
        format.codec == CodecID::CODEC_ID_AAC ||
        //format.codec == CodecID::CODEC_ID_ADPCM_G726 ||
        format.codec == CodecID::CODEC_ID_PCM_MULAW;
}

void QnAxisAudioTransmitter::setOutputFormat(const QnOutputAudioFormat& format)
{
    QnMutexLocker lock(&m_mutex);
    m_outputFormat = format;
    m_transcoder.reset(new QnFfmpegAudioTranscoder(format.codec));
    if (format.sampleRate != QnOutputAudioFormat::kDefaultSampleRate)
        m_transcoder->setSampleRate(format.sampleRate);
}

bool QnAxisAudioTransmitter::isInitialized() const
{
    QnMutexLocker lock(&m_mutex);
    return m_transcoder != nullptr;
}

QnAxisAudioTransmitter::QnAxisAudioTransmitter(QnSecurityCamResource* res) :
    QnAbstractAudioTransmitter(),
    m_resource(res),
    m_connectionEstablished(false),
    m_canProcessData(false),
    m_socket(new nx::network::TCPSocket())
{
}

QnAxisAudioTransmitter::~QnAxisAudioTransmitter()
{
    stop();
}

QString QnAxisAudioTransmitter::getAudioMimeType() const
{
    if (m_outputFormat.codec == CodecID::CODEC_ID_PCM_MULAW)
    {
        if (m_outputFormat.sampleRate == 8000)
            return lit("audio/g711");
        else
            return lit("audio/axis-mulaw-%1").arg((m_outputFormat.sampleRate * 8) / 1000);
    }
    else if (m_outputFormat.codec == CodecID::CODEC_ID_ADPCM_G726)
    {
        return lit("audio/G726-32");
    }
    else if (m_outputFormat.codec == CodecID::CODEC_ID_AAC)
    {
        return lit("audio/mpeg4-generic");
    }
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
    QAuthenticator auth = m_resource->getAuth();

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
            .arg(getAudioMimeType()))
        .append(lit("Content-Length: 999999999\r\n"))
        .append(lit("Connection: Keep-Alive\r\n"))
        .append(lit("Cache-Control: no-cache\r\n"))
        .append(lit("Authorization: %1\r\n")
            .arg(QString::fromLatin1(authHeaderBuf)))
        .append(lit("\r\n"));

    return transmitRequest;
}

int QnAxisAudioTransmitter::sendData(const char* data, int dataSize)
{
    int totalBytesSent = 0;
    while (dataSize > 0 && !m_needStop)
    {
        auto bytesSent = m_socket->send(data + totalBytesSent, dataSize);
        if (bytesSent <= 0)
            break; //< error occurred
        totalBytesSent += bytesSent;
        dataSize -= bytesSent;
    }
    return totalBytesSent;
}

bool QnAxisAudioTransmitter::establishConnection()
{
    const auto sockAddr = getSocketAddress();

    qDebug() << "Axis audio transmitter, Connecting to " << sockAddr.toString();

    m_socket->reopen();
    auto isConnected = m_socket->connect(sockAddr, kConnectionTimeoutMs);
    if (!isConnected)
        return false;

    auto transmitRequest = buildTransmitRequest();

    qDebug() << "Axis audio transmitter, sending post request" << QString(transmitRequest);

    int totalBytesSent = sendData(transmitRequest.constData(), transmitRequest.size());
    m_connectionEstablished = (totalBytesSent == transmitRequest.size());
    
    if (!m_connectionEstablished && !m_needStop)
        qDebug() << "Axis audio transmitter, error occured during connection establishing: "
                << SystemError::getLastOSErrorText();

    return m_connectionEstablished;
}

bool QnAxisAudioTransmitter::processAudioData(QnConstAbstractMediaDataPtr &data)
{
    if (!m_connectionEstablished && !establishConnection())
        return true; //< always return true. It means skip input data.

    if (!m_transcoder->isOpened() && !m_transcoder->open(data->context))
        return true; //< always return true. It means skip input data.
    
    QnAbstractMediaDataPtr transcoded;
    do 
    {
        m_transcoder->transcodePacket(data, &transcoded);
        data.reset();
        if (!transcoded)
            break;
        if (sendData(transcoded->data(), transcoded->dataSize()) != transcoded->dataSize())
        {
            if (!m_needStop)
            {
                qDebug() << "Axis audio transmitter, error occured during audio data transmission:"
                    << SystemError::getLastOSErrorText() << ", "
                    << "Error code: "
                    << SystemError::getLastOSErrorCode();
            }
            m_connectionEstablished = false;
        }
    } while (!m_needStop && transcoded);

    return true;
}

void QnAxisAudioTransmitter::pleaseStop()
{
    base_type::pleaseStop();
    m_socket->shutdown();
    m_connectionEstablished = false;
}
