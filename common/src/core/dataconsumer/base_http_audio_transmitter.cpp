#ifdef ENABLE_DATA_PROVIDERS

#include "base_http_audio_transmitter.h"

#include <nx/utils/std/future.h>

#include <core/resource/security_cam_resource.h>
#include <core/resource/media_server_resource.h>

namespace {

AVCodecID toFfmpegCodec(const QString& codec)
{
    if (codec == lit("AAC"))
        return AV_CODEC_ID_AAC;
    else if (codec == lit("G726"))
        return AV_CODEC_ID_ADPCM_G726;
    else if (codec == lit("MULAW"))
        return AV_CODEC_ID_PCM_MULAW;
    else if (codec == lit("ALAW"))
        return AV_CODEC_ID_PCM_ALAW;
    else if (codec == lit("PCM_S16LE"))
        return AV_CODEC_ID_PCM_S16LE;
    else
        return AV_CODEC_ID_NONE;
}

} // namespace

BaseHttpAudioTransmitter::BaseHttpAudioTransmitter(QnSecurityCamResource* res):
    QnAbstractAudioTransmitter(),
    m_resource(res),
    m_state(TransmitterState::WaitingForConnection),
    m_bitrateKbps(0),
    m_transcoder(nullptr),
    m_socket(nullptr),
    m_uploadMethod(nx::network::http::Method::post)
{
    connect(
        m_resource, &QnResource::parentIdChanged, this,
        [this]()
        {
            pleaseStop();
        },
        Qt::DirectConnection);
}

BaseHttpAudioTransmitter::~BaseHttpAudioTransmitter()
{
    stop();
    disconnect(m_resource, nullptr, this, nullptr);
}

void BaseHttpAudioTransmitter::pleaseStop()
{
    base_type::pleaseStop();
    m_wait.wakeOne();
}

void BaseHttpAudioTransmitter::endOfRun()
{
    base_type::endOfRun();
    m_socket.reset();
    m_state = TransmitterState::WaitingForConnection;
}

void BaseHttpAudioTransmitter::setOutputFormat(const QnAudioFormat& format)
{
    QnMutexLocker lock(&m_mutex);
    m_outputFormat = format;
}

void BaseHttpAudioTransmitter::setBitrateKbps(int value)
{
    QnMutexLocker lock(&m_mutex);
    m_bitrateKbps = value;
}

void BaseHttpAudioTransmitter::setAudioUploadHttpMethod(nx::network::http::StringType method)
{
    QnMutexLocker lock(&m_mutex);
    m_uploadMethod = method;
}

bool BaseHttpAudioTransmitter::isInitialized() const
{
    QnMutexLocker lock(&m_mutex);
    return m_transcoder != nullptr;
}

void BaseHttpAudioTransmitter::prepare()
{
    QnMutexLocker lock(&m_mutex);
    m_transcoder.reset(new QnFfmpegAudioTranscoder(toFfmpegCodec(m_outputFormat.codec())));
    m_transcoder->setSampleRate(m_outputFormat.sampleRate());
    if (m_bitrateKbps > 0)
        m_transcoder->setBitrate(m_bitrateKbps);
}

bool BaseHttpAudioTransmitter::processAudioData(const QnConstCompressedAudioDataPtr& audioData)
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
            auto res = sendData(transcoded);

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

bool BaseHttpAudioTransmitter::sendBuffer(nx::network::AbstractStreamSocket* socket, const char* buffer, size_t size)
{
    size_t bytesSent = 0;

    const char* currentPos = buffer;

    while (bytesSent < size)
    {
        auto res = socket->send(currentPos, (unsigned int)(size - bytesSent));
        if (res <= 0)
            return false;
        bytesSent += res;
        currentPos += res;
    }

    return true;
}

std::unique_ptr<nx::network::AbstractStreamSocket> BaseHttpAudioTransmitter::takeSocket(
    const nx::network::http::AsyncHttpClientPtr& httpClient) const
{
    //NOTE m_asyncHttpClient->takeSocket() can only be called within m_asyncHttpClient's aio thread
    std::unique_ptr<nx::network::AbstractStreamSocket> sock;
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


bool BaseHttpAudioTransmitter::startTransmission()
{
    m_state = TransmitterState::WaitingForConnection;

    QnResourcePtr mServer = m_resource->getParentServer();

    if (!mServer)
        return false;

    m_socket.reset();

    nx::network::http::AsyncHttpClientPtr httpClient = nx::network::http::AsyncHttpClient::create();
    prepareHttpClient(httpClient);

    QObject::connect(
        httpClient.get(),
        &nx::network::http::AsyncHttpClient::requestHasBeenSent,
        this,
        &BaseHttpAudioTransmitter::at_requestHeadersHasBeenSent,
        Qt::DirectConnection);

    QObject::connect(
        httpClient.get(),
        &nx::network::http::AsyncHttpClient::done,
        this,
        &BaseHttpAudioTransmitter::at_httpDone,
        Qt::DirectConnection);

    auto url = transmissionUrl();
    nx::network::http::StringType contentBody;

    QnMutexLocker lock(&m_mutex);

    if (m_uploadMethod == nx::network::http::Method::post)
    {
        httpClient
            ->doPost(
                url,
                contentType(),
                contentBody,
                !contentBody.isEmpty());
    }
    else if (m_uploadMethod == nx::network::http::Method::put)
    {
        httpClient
            ->doPut(
                url,
                contentType(),
                contentBody);
    }
    else
    {
        NX_ASSERT(false, lit("Only POST and PUT methods are allowed."));
    }

    m_timer.restart();
    while (m_state == TransmitterState::WaitingForConnection &&
        m_timer.elapsed() < transmissionTimeout().count() && !m_needStop)
    {
        auto waitDuration = transmissionTimeout().count() - m_timer.elapsed();
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
        lock.unlock();
        httpClient->pleaseStopSync();
        m_state = TransmitterState::Failed;
    }

    return m_socket != nullptr;

}

void BaseHttpAudioTransmitter::at_requestHeadersHasBeenSent(
    nx::network::http::AsyncHttpClientPtr httpClient,
    bool isRetryAfterUnauthorizedResponse)
{
    if (isReadyForTransmission(httpClient, isRetryAfterUnauthorizedResponse))
    {
        QnMutexLocker lock(&m_mutex);
        m_state = TransmitterState::ReadyForTransmission;
        m_wait.wakeOne();
    }
}

void BaseHttpAudioTransmitter::at_httpDone(nx::network::http::AsyncHttpClientPtr httpClient)
{
    if (httpClient->state() == nx::network::http::AsyncClient::State::sFailed)
    {
        QnMutexLocker lock(&m_mutex);
        m_state = TransmitterState::Failed;
        m_wait.wakeOne();
    }
}

#endif // ENABLE_DATA_PROVIDERS
