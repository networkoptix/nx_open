#include "axis_audio_transmitter.h"
#include <network/router.h>
#include <http/custom_headers.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>

namespace
{
    const QString kAxisAudioTransmitUrl("/axis-cgi/audio/transmit.cgi");
    const uint64_t kAxisBodySourceContentLength = 999999999;
    const quint64 kTransmissionTimeout = 3000;

    CodecID toFfmpegCodec(const QString& codec)
    {
        if (codec == "AAC")
            return CODEC_ID_AAC;
        else if (codec == "G726")
            return CODEC_ID_ADPCM_G726;
        else if (codec == "MULAW")
            return CODEC_ID_PCM_MULAW;
        else
            return CODEC_ID_NONE;
    }
}


QnAxisAudioTransmitter::QnAxisAudioTransmitter(QnSecurityCamResource* res) :
    QnAbstractAudioTransmitter(),
    m_resource(res),
    m_noAuth(false),
    m_state(TransmitterState::WaitingForConnection)
{

}

QnAxisAudioTransmitter::~QnAxisAudioTransmitter()
{
    stop();
}


void QnAxisAudioTransmitter::pleaseStop()
{
    base_type::pleaseStop();
    m_wait.wakeOne();
}


void QnAxisAudioTransmitter::setOutputFormat(const QnAudioFormat& format)
{
    QnMutexLocker lock(&m_mutex);
    m_outputFormat = format;
}

nx_http::StringType QnAxisAudioTransmitter::mimeType() const
{
    if (m_outputFormat.codec() == "MULAW")
    {
        if (m_outputFormat.sampleRate() == 8000)
            return QByteArray("audio/basic");
        else
            return lit("audio/axis-mulaw-%1")
                .arg((m_outputFormat.sampleRate() * 8) / 1000)
                .toLatin1();
    }
    else if (m_outputFormat.codec() == "G726")
    {
        return QByteArray("audio/G726-32");
    }
    else if (m_outputFormat.codec() == "AAC")
    {
        return QByteArray("audio/mpeg4-generic");
    }
    else
    {
        return QByteArray("audio/basic");
    }
}



bool QnAxisAudioTransmitter::isCompatible(const QnAudioFormat& format) const
{
    return
        format.codec() == "AAC" ||
        //format.codec() == "G726" ||
        format.codec() == "MULAW";
}

bool QnAxisAudioTransmitter::isInitialized() const
{
    QnMutexLocker lock(&m_mutex);
    return m_transcoder != nullptr;
}

void QnAxisAudioTransmitter::prepare()
{
    QnMutexLocker lock(&m_mutex);
    m_transcoder.reset(new QnFfmpegAudioTranscoder(toFfmpegCodec(m_outputFormat.codec())));
    m_transcoder->setSampleRate(m_outputFormat.sampleRate());
    m_state = TransmitterState::WaitingForConnection;
}

void QnAxisAudioTransmitter::at_requestHeadersHasBeenSent(
    nx_http::AsyncHttpClientPtr http,
    bool isRetryAfterUnauthorizedResponse)
{
    QN_UNUSED(http);
    if (isRetryAfterUnauthorizedResponse || m_noAuth)
    {
        m_state = TransmitterState::ReadyForTransmission;
        m_wait.wakeOne();
    }
}

void QnAxisAudioTransmitter::at_httpDone(nx_http::AsyncHttpClientPtr http)
{
    if (http->state() == nx_http::AsyncHttpClient::State::sFailed)
    {
        m_state = TransmitterState::Failed;
        m_wait.wakeOne();
    }
}


bool QnAxisAudioTransmitter::startTransmission()
{
    m_state = TransmitterState::WaitingForConnection;

    QUrl url(m_resource->getUrl());

    QnResourcePtr mServer = m_resource->getParentServer();

    if (!mServer)
        return false;

    m_socket.clear();

    auto auth = m_resource->getAuth();
    m_noAuth = auth.user().isEmpty() && auth.password().isEmpty();

    nx_http::AsyncHttpClientPtr httpClient = nx_http::AsyncHttpClient::create();
    httpClient->setUserName(auth.user());
    httpClient->setUserPassword(auth.password());
    httpClient->setDisablePrecalculatedAuthorization(true);

    QObject::connect(
        httpClient.get(),
        &nx_http::AsyncHttpClient::requestHasBeenSent,
        this,
        &QnAxisAudioTransmitter::at_requestHeadersHasBeenSent,
        Qt::DirectConnection);

    QObject::connect(
        httpClient.get(),
        &nx_http::AsyncHttpClient::done,
        this,
        &QnAxisAudioTransmitter::at_httpDone,
        Qt::DirectConnection);

    url.setScheme(lit("http"));
    if (url.host().isEmpty())
        url.setHost(m_resource->getHostAddress());

    url.setPath(kAxisAudioTransmitUrl);

    nx_http::StringType contentType(mimeType());
    nx_http::StringType contentBody;

    httpClient
        ->doPost(
            url,
            contentType,
            contentBody,
            true);

    m_timer.restart();
    QnMutexLocker lock(&m_mutex);
    while (m_state == TransmitterState::WaitingForConnection &&
           m_timer.elapsed() < kTransmissionTimeout && !m_needStop)
    {
        auto waitDuration = kTransmissionTimeout - m_timer.elapsed();
        if (waitDuration <= 0)
            break;

        m_wait.wait(lock.mutex(), waitDuration);
    }

    if (m_state == TransmitterState::ReadyForTransmission)
    {
        m_socket = httpClient->takeSocket();
        m_socket->setNonBlockingMode(true);
        return true;
    }
    else
    {
        m_state = TransmitterState::Failed;
        return false;
    }

    httpClient->terminate();
}

bool QnAxisAudioTransmitter::sendData(
    QSharedPointer<AbstractStreamSocket> socket,
    const char* buffer,
    size_t size)
{
    size_t bytesSent = 0;

    char* currentPos = const_cast<char*>(buffer);

    while ( bytesSent < size )
    {
        auto res = socket->send(currentPos, size - bytesSent);
        if(res > 0)
            bytesSent += res;
        else
            return false;
    }

    return true;
}

bool QnAxisAudioTransmitter::processAudioData(QnConstAbstractMediaDataPtr &data)
{
    if (m_state != TransmitterState::ReadyForTransmission && !startTransmission())
        return true;

    if (!m_transcoder->isOpened() && !m_transcoder->open(data->context))
        return true; //< always return true. It means skip input data.

    if (!m_socket)
        return true;
    QnAbstractMediaDataPtr transcoded;
    do
    {
        m_transcoder->transcodePacket(data, &transcoded);
        data.reset();

        if (!m_needStop && transcoded)
        {
            auto res = sendData(
                m_socket,
                transcoded->data(),
                transcoded->dataSize());

            if (!res)
            {
                m_state = TransmitterState::Failed;
                m_socket.clear();
                break;
            }
        }

    } while (!m_needStop && transcoded);

    return true;
}

