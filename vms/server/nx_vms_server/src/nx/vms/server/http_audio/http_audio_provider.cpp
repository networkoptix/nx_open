#include "http_audio_provider.h"
#include <camera/video_camera.h>

using namespace std::placeholders;

namespace nx::vms::server::http_audio {

HttpAudioProvider::HttpAudioProvider(
    nx::network::aio::AsyncChannelPtr socket):
    QnAbstractStreamDataProvider(QnResourcePtr()),
    m_syncReader(std::move(socket))
{
}

HttpAudioProvider::~HttpAudioProvider()
{
    m_syncReader.cancel();
    stop();
}

bool HttpAudioProvider::openStream(const FfmpegAudioDemuxer::StreamConfig* config)
{
    FfmpegIoContextPtr ioContext = std::make_unique<FfmpegIoContext>(1024 * 16, false);
    ioContext->readHandler = std::bind(&SyncReader::read, &m_syncReader, _1, _2);
    return m_demuxer.open(std::move(ioContext), config);
}

void HttpAudioProvider::run()
{
    while (!needToStop())
    {
        if (!dataCanBeAccepted())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
    eofData->dataProvider = this;
    putData(eofData);
}

} // namespace nx::vms::server::http_audio

