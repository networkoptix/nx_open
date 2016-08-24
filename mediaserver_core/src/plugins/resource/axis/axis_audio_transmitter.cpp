#include "axis_audio_transmitter.h"
#include <network/router.h>
#include <http/custom_headers.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/std/future.h>

namespace
{
    static const QString kAxisAudioTransmitUrl("/axis-cgi/audio/transmit.cgi");
    static const uint64_t kAxisBodySourceContentLength = 999999999;
    static const quint64 kTransmissionTimeout = 3000;

    AVCodecID toFfmpegCodec(const QString& codec)
    {
        if (codec == "AAC")
            return AV_CODEC_ID_AAC;
        else if (codec == "G726")
            return AV_CODEC_ID_ADPCM_G726;
        else if (codec == "MULAW")
            return AV_CODEC_ID_PCM_MULAW;
        else
            return AV_CODEC_ID_NONE;
    }
}


QnAxisAudioTransmitter::QnAxisAudioTransmitter(QnSecurityCamResource* res):
    QnAbstractAudioTransmitter(),
    m_resource(res),
    m_noAuth(false),
    m_state(TransmitterState::WaitingForConnection)
{
    connect(m_resource, &QnResource::parentIdChanged, this, [this]()
    {
        pleaseStop();
    }, Qt::DirectConnection);
}

QnAxisAudioTransmitter::~QnAxisAudioTransmitter()
{
    stop();
    disconnect(m_resource, nullptr, this, nullptr);
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

std::unique_ptr<AbstractStreamSocket> QnAxisAudioTransmitter::takeSocket(const nx_http::AsyncHttpClientPtr& httpClient)
{
    //NOTE m_asyncHttpClient->takeSocket() can only be called within m_asyncHttpClient's aio thread
    std::unique_ptr<AbstractStreamSocket> sock;
    nx::utils::promise<void> socketTakenPromise;
    httpClient->dispatch(
        [this, &sock, &socketTakenPromise, &httpClient]()
    {
        sock = std::move(httpClient->takeSocket());
        socketTakenPromise.set_value();
    });
    socketTakenPromise.get_future().wait();

    if (!sock || !sock->setNonBlockingMode(false))
        return nullptr;
    return sock;
}

bool QnAxisAudioTransmitter::startTransmission()
{
    m_state = TransmitterState::WaitingForConnection;

    QUrl url(m_resource->getUrl());

    QnResourcePtr mServer = m_resource->getParentServer();

    if (!mServer)
        return false;

    m_socket.reset();

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
           m_timer.elapsed() < (qint64)kTransmissionTimeout && !m_needStop)
    {
        auto waitDuration = kTransmissionTimeout - m_timer.elapsed();
        if (waitDuration <= 0)
            break;

        m_wait.wait(lock.mutex(), waitDuration);
    }

    if (m_state == TransmitterState::ReadyForTransmission)
    {
        m_socket = takeSocket(httpClient);
    }
    else
    {
        m_state = TransmitterState::Failed;
        httpClient->terminate();
    }

    return m_socket != nullptr;

}

bool QnAxisAudioTransmitter::sendData(
    AbstractStreamSocket* socket,
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

bool QnAxisAudioTransmitter::processAudioData(const QnConstCompressedAudioDataPtr& audioData)
{
    if (m_state != TransmitterState::ReadyForTransmission && !startTransmission())
        return true;

    if (!m_transcoder->isOpened() && !m_transcoder->open(audioData->context))
        return true; //< always return true. It means skip input data.

    if (!m_socket)
        return true;
    QnAbstractMediaDataPtr transcoded;
    auto data = audioData;
    do
    {
        m_transcoder->transcodePacket(data, &transcoded);
        data.reset();

        if (!m_needStop && transcoded)
        {
            auto res = sendData(
                m_socket.get(),
                transcoded->data(),
                transcoded->dataSize());

            if (!res)
            {
                m_state = TransmitterState::Failed;
                m_socket.reset();
                break;
            }
        }

    } while (!m_needStop && transcoded);

    return true;
}

