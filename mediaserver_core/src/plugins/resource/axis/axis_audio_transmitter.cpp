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
    m_httpClient(nx_http::AsyncHttpClient::create()),
    m_noAuth(false),
    m_state(TransmitterState::WaitingForConnection)
{

}

QnAxisAudioTransmitter::~QnAxisAudioTransmitter()
{
    if (m_httpClient)
        m_httpClient->terminate();
    stop();
}


void QnAxisAudioTransmitter::pleaseStop()
{
    base_type::pleaseStop();

    if (m_httpClient)
        m_httpClient->terminate();
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
    if (m_httpClient)
    {
        m_httpClient->terminate();
        m_httpClient.reset();
    }
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

        if (m_socket)
        {
            m_socket->cancelAsyncIO();
            m_socket.clear();
        }

        if (m_httpClient)
        {
            m_httpClient->terminate();
            m_httpClient.reset();
        }

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

    if (m_httpClient)
        m_httpClient->terminate();

    m_httpClient = nx_http::AsyncHttpClient::create();

    if (m_socket)
    {
        m_socket->cancelAsyncIO();
        m_socket.clear();
    }

    if (mServer->getId() != qnCommon->moduleGUID())
    {
        // proxy request to foreign camera
        auto route = QnRouter::instance()->routeTo(mServer->getId());
        if (route.addr.isNull())
            return false;

        m_httpClient->addAdditionalHeader(
            Qn::CAMERA_GUID_HEADER_NAME,
            m_resource->getId().toByteArray());

        url.setHost(route.addr.address.toString());
        url.setPort(route.addr.port);
    }

    auto auth = m_resource->getAuth();
    m_noAuth = auth.user().isEmpty() && auth.password().isEmpty();

    m_httpClient->setUserName(auth.user());
    m_httpClient->setUserPassword(auth.password());
    m_httpClient->setDisablePrecalculatedAuthorization(true);

    QObject::connect(
        m_httpClient.get(),
        &nx_http::AsyncHttpClient::requestHasBeenSent,
        this,
        &QnAxisAudioTransmitter::at_requestHeadersHasBeenSent,
        Qt::DirectConnection);

    QObject::connect(
        m_httpClient.get(),
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

    m_httpClient
        ->doPost(
            url,
            contentType,
            contentBody,
            true);

    m_timer.restart();
    QnMutexLocker lock(&m_mutex);
    while (m_state == TransmitterState::WaitingForConnection &&
           m_timer.elapsed() < kTransmissionTimeout )
    {
        auto waitDuration = kTransmissionTimeout - m_timer.elapsed();
        if (waitDuration <= 0)
            break;

        m_wait.wait(lock.mutex(), waitDuration);
    }

    if (m_state == TransmitterState::ReadyForTransmission)
    {
        return true;
    }
    else
    {
        m_state = TransmitterState::Failed;
        return false;
    }

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
        m_socket = m_httpClient->takeSocket();

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

