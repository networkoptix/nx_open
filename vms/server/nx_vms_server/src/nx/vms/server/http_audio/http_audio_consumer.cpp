
#include "http_audio_consumer.h"

namespace {

using namespace nx::vms::server::http_audio;

SystemError::ErrorCode syncWrite(
    const nx::network::aio::AsyncChannelPtr& socket, const char* buffer, int size)
{
    if (!socket)
        return SystemError::notConnected;

    SystemError::ErrorCode error;
    while(size > 0)
    {
        std::promise<void> done;
        std::size_t bytesWritten;
        nx::Buffer nxBuffer(buffer, size);
        socket->sendAsync(nxBuffer,
            [&error, &bytesWritten, &done](
                SystemError::ErrorCode errorCode, std::size_t bytesWrittenSocket)
            {
                error = errorCode;
                bytesWritten = bytesWrittenSocket;
                done.set_value();
            });
        done.get_future().wait();
        if (error != SystemError::noError)
            return error;

        buffer += bytesWritten;
        size -= bytesWritten;
        NX_DEBUG(NX_SCOPE_TAG, "written %1, bytes left %2", bytesWritten, size);
    }
    return error;
}

}

namespace nx::vms::server::http_audio {

HttpAudioConsumer::HttpAudioConsumer(nx::network::aio::AsyncChannelPtr socket):
    QnAbstractDataConsumer(32),
    m_socket(std::move(socket))
{
}

HttpAudioConsumer::~HttpAudioConsumer()
{
    stop();
}


void HttpAudioConsumer::putData(const QnAbstractDataPacketPtr& data)
{
    const auto mediaData = std::dynamic_pointer_cast<QnAbstractMediaData>(data);
    if (!mediaData)
        return;

    if (mediaData->dataType != QnAbstractMediaData::AUDIO)
        return;

    QnAbstractDataConsumer::putData(data);
}

bool HttpAudioConsumer::initializeTranscoder(const QnAbstractDataPacketPtr& data)
{
    m_transcoder = std::make_unique<QnFfmpegTranscoder>(QnFfmpegTranscoder::Config(), nullptr);
    m_transcoder->setContainer("adts");
    m_transcoder->setAudioCodec(AV_CODEC_ID_AAC, QnTranscoder::TM_FfmpegTranscode);
    int status = m_transcoder->open(
        nullptr, std::dynamic_pointer_cast<const QnCompressedAudioData>(data));
    if (status != 0)
    {
        NX_WARNING(this, "Failed to open audio transcoder, error: %1", status);
        return false;
    }
    return true;
}

bool HttpAudioConsumer::processData(const QnAbstractDataPacketPtr& data)
{
    if (!transcodeAndSend(data))
    {
        pleaseStop();
        return false;
    }
    return true;
}

bool HttpAudioConsumer::transcodeAndSend(const QnAbstractDataPacketPtr& data)
{
    const auto mediaData = std::dynamic_pointer_cast<QnAbstractMediaData>(data);
    if (!mediaData)
    {
        NX_WARNING(this, "Invalid media data");
        return false;
    }

    if (!m_transcoder && !initializeTranscoder(mediaData))
    {
        NX_WARNING(this, "Failed to initialize audio transcoder");
        return false;
    }

    QnByteArray transcodedPacket(32, 0);
    int status = m_transcoder->transcodePacket(mediaData, &transcodedPacket);
    if (status != 0)
    {
        NX_WARNING(this, "Failed to transcode audio packet. Error code: %1", status);
        return false;
    }
    if (transcodedPacket.size() == 0)
        return true;


    SystemError::ErrorCode error = syncWrite(
        m_socket, (const char*)transcodedPacket.data(), transcodedPacket.size());

    if (error != SystemError::noError)
    {
        if (error != SystemError::connectionAbort)
        {
            NX_WARNING(this, "Failed to write to socket. Error code: %1",
                SystemError::toString(error));
        }
        m_socket.reset();
        return false;
    }
    return true;
}

} // namespace nx::vms::server::http_audio
