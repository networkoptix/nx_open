#pragma once

#include <vector>
#include <functional>

#include "codec.h"
#include "scaler.h"
#include "packet.h"
#include "frame.h"
#include "camera/codec_parameters.h"
#include "camera/timestamp_mapper.h"

namespace nx::usb_cam::ffmpeg {

class VideoTranscoder
{
public:
    using CurrentTimeGetter = std::function<int64_t()>;

public:
    void setTimeGetter(const CurrentTimeGetter& timeGetter) { m_timeGetter = timeGetter; }
    int initialize(AVCodecParameters* decoderCodecPar, const CodecParameters& codecParams);
    int transcode(const ffmpeg::Packet* source, std::shared_ptr<ffmpeg::Packet>& target);
    void uninitialize();

private:
    std::shared_ptr<ffmpeg::Frame> decode(const ffmpeg::Packet * packet);
    int encode(const ffmpeg::Frame* frame, ffmpeg::Packet * outPacket);
    bool shouldDrop(const ffmpeg::Frame * frame);

    int initializeDecoder(AVCodecParameters* codecPar);
    int initializeEncoder(const CodecParameters& codecParams);
    void setEncoderOptions(ffmpeg::Codec* encoder);

private:
    CurrentTimeGetter m_timeGetter;
    CodecParameters m_codecParams;
    TimestampMapper m_encoderTimestamps;
    TimestampMapper m_decoderTimestamps;
    std::unique_ptr<ffmpeg::Codec> m_decoder;
    std::unique_ptr<ffmpeg::Codec> m_encoder;
    Scaler m_scaler;
    int64_t m_lastTimestamp = 0;
};

} // namespace nx::usb_cam::ffmpeg
