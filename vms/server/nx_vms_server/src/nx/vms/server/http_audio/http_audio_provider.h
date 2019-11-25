#pragma once

#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/vms/server/http_audio/sync_reader.h>
#include <nx/vms/server/http_audio/ffmpeg_audio_demuxer.h>
#include <camera/camera_fwd.h>

namespace nx::vms::server::http_audio {

class HttpAudioProvider:
    public QnAbstractStreamDataProvider
{
public:
    HttpAudioProvider(nx::network::aio::AsyncChannelPtr socket);
    ~HttpAudioProvider();

    bool openStream(const FfmpegAudioDemuxer::StreamConfig* config);

private:
    virtual void run() override;

private:
    SyncReader m_syncReader;
    FfmpegAudioDemuxer m_demuxer;
};

} // namespace nx::vms::server::http_audio
