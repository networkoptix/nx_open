#pragma once

#include <string>

#include <nx/vms/server/http_audio/ffmpeg_io_context.h>
#include <nx/streaming/media_data_packet.h>

struct AVFormatContext;

namespace nx::vms::server::http_audio {

struct UtcTimestamp
{
    int64_t getTimestamp(int64_t timestamp, const AVRational& timescale);

private:
    int64_t m_firstTimestamp = -1;
    int64_t m_utcStartTime = -1;
    const AVRational m_usecTimescale {1, 1000000};
};

class FfmpegAudioDemuxer
{
public:
    struct StreamConfig
    {
        std::string format;
        std::string sampleRate;
        std::string channelsNumber;
    };

public:
    FfmpegAudioDemuxer() = default;
    FfmpegAudioDemuxer(const FfmpegAudioDemuxer&) = delete;
    FfmpegAudioDemuxer& operator=(const FfmpegAudioDemuxer&) = delete;
    ~FfmpegAudioDemuxer();

    bool open(FfmpegIoContextPtr ioContext, const StreamConfig* config = nullptr);
    void close();
    QnAbstractMediaDataPtr getNextData();

private:
    FfmpegIoContextPtr m_ioContext;
    UtcTimestamp m_utcTimestamp;
    AVFormatContext* m_formatContext = nullptr;
};

} // namespace nx::vms::server::http_audio
