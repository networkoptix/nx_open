#include "acti_audio_transmitter.h"

namespace {

const QString kEncoderCgiPath = lit("/cgi-bin/cmd/encoder");
const QString kEncoderCgiQuery = lit("SEND_AUDIO");
const QString kMultipartBoundary = lit("---------Nx2WayAudioBoundary");
const AVCodecID kTransmissionCodecId = AV_CODEC_ID_PCM_S16LE;
const QString kAudioTransmissionRequestMimeType = lit("multipart/x-mixed-replace");
const QString kAudioMimeType = lit("audio/pcm");
const int kSingleChunkLength = 1024;
const int kSampleRate = 8000;
const int kBitsPerSample = 16;
const std::chrono::milliseconds kTransmissionTimeout(4000);

} // namespace

ActiAudioTransmitter::ActiAudioTransmitter(QnSecurityCamResource* resource):
    BaseHttpAudioTransmitter(resource)
{
    m_chunkPrefixBuffer = (lit("%1\r\n").arg(kMultipartBoundary)
        + lit("Content-Type: %1\r\n").arg(kAudioMimeType)
        + lit("Content-Length: %1\r\n\r\n").arg(kSingleChunkLength)).toLatin1();
}

bool ActiAudioTransmitter::isCompatible(const QnAudioFormat& format) const
{
    auto codec = format.codec().toLower();

    return format.sampleSize() == kBitsPerSample
        && format.sampleRate() == kSampleRate
        && (codec == lit("pcm") || codec == kAudioMimeType);
}

void ActiAudioTransmitter::prepare()
{
    // ACTi supports only 8000kHz PCM_16LE format.
    QnMutexLocker lock(&m_mutex);
    m_transcoder.reset(new QnFfmpegAudioTranscoder(kTransmissionCodecId));
    m_transcoder->setSampleRate(kSampleRate);
}

bool ActiAudioTransmitter::sendData(const QnAbstractMediaDataPtr& audioData)
{
    std::size_t totalBytesAvailable = audioData->dataSize() + m_buffer.size();
    std::size_t bufferOffset = 0;
    
    bool result = false;

    while (totalBytesAvailable >= kSingleChunkLength)
    {
        base_type::sendBuffer(
            m_socket.get(),
            m_chunkPrefixBuffer.constData(),
            m_chunkPrefixBuffer.size());

        if (m_buffer.size() != 0)
        {
            // There is some data from previous packets.
            result = base_type::sendBuffer(m_socket.get(), m_buffer.constData(), m_buffer.size());
            if (!result)
                return false;

            auto chunkRest = kSingleChunkLength - m_buffer.size();
            m_buffer.clear();

            if (chunkRest > 0)
            {
                result = base_type::sendBuffer(m_socket.get(), audioData->data(), chunkRest);

                if (!result)
                    return false;

                bufferOffset = chunkRest;
            }
        }
        else
        {
            result = base_type::sendBuffer(
                m_socket.get(),
                audioData->data() + bufferOffset,
                kSingleChunkLength);

            if (!result)
                return false;

            bufferOffset += kSingleChunkLength;
        }

        totalBytesAvailable -= kSingleChunkLength;
    }

    m_buffer.append(audioData->data() + bufferOffset, totalBytesAvailable);
    return true;
}

void ActiAudioTransmitter::prepareHttpClient(const nx_http::AsyncHttpClientPtr& httpClient)
{
    auto auth = m_resource->getAuth();

    httpClient->setUserName(auth.user());
    httpClient->setUserPassword(auth.password());
    httpClient->setDisablePrecalculatedAuthorization(false);

    // There is a bug in ACTi authorization for SEND_AUDIO request:
    // if we don't add authorization info to the first request all subsequent 
    // requests will fail with "401 Unauthorized", so we have to add authorization 
    // to the first request.
    httpClient->setAuthType(nx_http::AuthType::authBasic);
}

bool ActiAudioTransmitter::isReadyForTransmission(
    nx_http::AsyncHttpClientPtr /*httpClient*/,
    bool /*isRetryAfterUnauthorizedResponse*/) const
{
    return true;
}

QUrl ActiAudioTransmitter::transmissionUrl() const
{
    QUrl url(m_resource->getUrl());
    url.setPath(kEncoderCgiPath);
    url.setQuery(kEncoderCgiQuery);

    return url;
}

std::chrono::milliseconds ActiAudioTransmitter::transmissionTimeout() const
{
    return kTransmissionTimeout;
}

nx_http::StringType ActiAudioTransmitter::contentType() const
{
    nx_http::StringType contentType;
    contentType.append(kAudioTransmissionRequestMimeType);
    contentType.append(lit(";boundary=%1").arg(kMultipartBoundary));

    return contentType;
}

