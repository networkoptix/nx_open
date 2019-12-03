#include "async_channel_audio_provider.h"
#include <camera/video_camera.h>

using namespace std::placeholders;

namespace nx::vms::server::http_audio {

AsyncChannelAudioProvider::AsyncChannelAudioProvider(
    nx::network::aio::AsyncChannelPtr socket,
    const std::optional<FfmpegAudioDemuxer::StreamConfig>& config):
    QnAbstractStreamDataProvider(QnResourcePtr()),
    m_config(config),
    m_syncReader(std::move(socket))
{
}

AsyncChannelAudioProvider::~AsyncChannelAudioProvider()
{
    m_syncReader.cancel();
    stop();
}

bool AsyncChannelAudioProvider::openStream()
{
    FfmpegIoContextPtr ioContext = std::make_unique<FfmpegIoContext>(1024 * 16, false);
    ioContext->readHandler = std::bind(&SyncReader::read, &m_syncReader, _1, _2);
    return m_demuxer.open(std::move(ioContext), m_config);
}

void AsyncChannelAudioProvider::run()
{
    if (!openStream())
    {
        NX_WARNING(this, "Failed to open audio stream, close connection");
        return;
    }
    while (!needToStop())
    {
        if (!dataCanBeAccepted())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        QnAbstractMediaDataPtr data = m_demuxer.getNextData();
        if (!data)
            break;

        if (data->dataType != QnAbstractMediaData::DataType::AUDIO)
            continue;

        data->dataProvider = this;
        putData(std::move(data));
    }
    QnAbstractMediaDataPtr eofData(new QnEmptyMediaData());
    eofData->flags.setFlag(QnAbstractMediaData::MediaFlags_AfterEOF);
    eofData->dataProvider = this;
    putData(eofData);
}

} // namespace nx::vms::server::http_audio

