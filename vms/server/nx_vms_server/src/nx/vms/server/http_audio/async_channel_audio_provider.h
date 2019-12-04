#pragma once

#include <optional>

#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/vms/server/http_audio/sync_reader.h>
#include <nx/vms/server/http_audio/ffmpeg_audio_demuxer.h>
#include <camera/camera_fwd.h>

namespace nx::vms::server::http_audio {

class AsyncChannelAudioProvider:
    public QnAbstractStreamDataProvider
{
public:
    AsyncChannelAudioProvider(
        nx::network::aio::AsyncChannelPtr socket,
        const std::optional<FfmpegAudioDemuxer::StreamConfig>& config);
    ~AsyncChannelAudioProvider();

private:
    virtual void run() override;
    bool openStream();

private:
    std::optional<FfmpegAudioDemuxer::StreamConfig> m_config;
    SyncReader m_syncReader;
    FfmpegAudioDemuxer m_demuxer;
};

} // namespace nx::vms::server::http_audio
