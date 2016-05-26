#include "axis_audio_transmitter.h"
#include <network/router.h>
#include <http/custom_headers.h>
#include <common/common_module.h>

namespace
{
    const QString kAxisAudioTransmitUrl("/axis-cgi/audio/transmit.cgi");
    const uint64_t kAxisBodySourceContentLength = 999999999;
    const int kConnectionTimeoutMs = 3000;

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


QnAxisAudioMsgBodySource::QnAxisAudioMsgBodySource() :
    m_waitingForNextData(false)
{
}

void QnAxisAudioMsgBodySource::setOutputFormat(const QnAudioFormat& format)
{
    m_outputFormat = format;
}

nx_http::StringType QnAxisAudioMsgBodySource::mimeType() const
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

boost::optional<uint64_t> QnAxisAudioMsgBodySource::contentLength() const
{
    return boost::optional<uint64_t>(kAxisBodySourceContentLength);
}

bool  QnAxisAudioMsgBodySource::isWaitingForData() const
{
    QnMutexLocker lock(&m_mutex);
    return m_waitingForNextData;
}

void QnAxisAudioMsgBodySource::putData(const nx_http::BufferType& data)
{
    QnMutexLocker lock(&m_mutex);
    m_callback(SystemError::noError, data);
    m_waitingForNextData = false;
}

void QnAxisAudioMsgBodySource::readAsync(ReadCallbackType completionHandler)
{
    QnMutexLocker lock(&m_mutex);
    m_waitingForNextData = true;
    m_callback = completionHandler;
}







QnAxisAudioTransmitter::QnAxisAudioTransmitter(QnSecurityCamResource* res) :
    QnAbstractAudioTransmitter(),
    m_resource(res),
    m_bodySource(new QnAxisAudioMsgBodySource()),
    m_httpClient(nx_http::AsyncHttpClient::create())
{
}

QnAxisAudioTransmitter::~QnAxisAudioTransmitter()
{
    m_httpClient->terminate();
    stop();
}


void QnAxisAudioTransmitter::pleaseStop()
{
    base_type::pleaseStop();

    m_httpClient->terminate();
    m_dataTransmissionStarted = false;
}

bool QnAxisAudioTransmitter::isCompatible(const QnAudioFormat& format) const
{
    return
        format.codec() == "AAC" ||
        //format.codec() == "G726" ||
        format.codec() == "MULAW";
}

void QnAxisAudioTransmitter::setOutputFormat(const QnAudioFormat& format)
{
    QnMutexLocker lock(&m_mutex);
    m_bodySource->setOutputFormat(format);
    m_outputFormat = format;
    m_transcoder.reset(new QnFfmpegAudioTranscoder(toFfmpegCodec(format.codec())));
    if (format.sampleRate() > 0)
        m_transcoder->setSampleRate(format.sampleRate());
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
    m_dataTransmissionStarted = false;
}


bool QnAxisAudioTransmitter::startTransmission()
{
    QUrl url(m_resource->getUrl());

    QnResourcePtr mServer = m_resource->getParentServer();
    if (!mServer)
        return false;
    if (mServer->getId() != qnCommon->moduleGUID())
    {
        // proxy request to foreign camera
        auto route = QnRouter::instance()->routeTo(mServer->getId());
        if (route.addr.isNull())
            return false;

        m_httpClient->addAdditionalHeader(Qn::CAMERA_GUID_HEADER_NAME, m_resource->getId().toByteArray());

        url.setHost(route.addr.address.toString());
        url.setPort(route.addr.port);
    }

    auto auth = m_resource->getAuth();

    m_httpClient->setUserName(auth.user());
    m_httpClient->setUserPassword(auth.password());
    m_transcodedBuffer.truncate(0);

    url.setScheme(lit("http"));
    if (url.host().isEmpty())
        url.setHost(m_resource->getHostAddress());

    url.setPath(kAxisAudioTransmitUrl);
    return m_dataTransmissionStarted = m_httpClient->doPost(url, m_bodySource);
}

bool QnAxisAudioTransmitter::processAudioData(QnConstAbstractMediaDataPtr &data)
{
    if(!m_dataTransmissionStarted && !startTransmission())
        return true;

    if (!m_transcoder->isOpened() && !m_transcoder->open(data->context))
        return true; //< always return true. It means skip input data.

    QnAbstractMediaDataPtr transcoded;
    do
    {
        m_transcoder->transcodePacket(data, &transcoded);
        data.reset();

        if (!transcoded)
        {
            if(
                !m_needStop &&
                m_bodySource->isWaitingForData() &&
                !m_transcodedBuffer.isEmpty())
            {
                m_bodySource->putData(m_transcodedBuffer);
                m_transcodedBuffer.truncate(0);
            }
        }
        else
        {
            m_transcodedBuffer.append(transcoded->data(), transcoded->dataSize());
        }

    } while (!m_needStop && transcoded);

    return true;
}

