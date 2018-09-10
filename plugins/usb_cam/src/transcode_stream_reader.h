#pragma once

#include "stream_reader.h"

#include <map>
#include <deque>

#include "camera/buffered_stream_consumer.h"
#include "camera/timestamp_mapper.h"

namespace nx { namespace ffmpeg { class Codec; } }
namespace nx { namespace ffmpeg { class Frame; } }

namespace nx {
namespace usb_cam {

//!Transfers or transcodes packets from USB webcameras and streams them
class TranscodeStreamReader : public StreamReaderPrivate
{
public:
    TranscodeStreamReader(
        int encoderIndex,
        const CodecParameters& codecParams,
        const std::shared_ptr<Camera>& camera);
    virtual ~TranscodeStreamReader();

    virtual int getNextData( nxcip::MediaDataPacket** lpPacket ) override;
    virtual void interrupt() override;

    virtual void setFps(float fps) override;
    virtual void setResolution(const nxcip::Resolution& resolution) override;
    virtual void setBitrate(int bitrate) override;

private:
    enum StreamState
    {
        csOff,
        csInitialized,
        csModified
    };
    StreamState m_cameraState;
    int m_retries;
    int m_initCode;

    uint64_t m_lastVideoTimeStamp;
    uint64_t m_timePerFrame;
    std::shared_ptr<BufferedVideoFrameConsumer> m_videoFrameConsumer;
    std::vector<std::shared_ptr<ffmpeg::Packet>> m_videoPackets;
    
    std::unique_ptr<nx::ffmpeg::Codec> m_encoder;
    std::unique_ptr<ffmpeg::Frame> m_scaledFrame;

    TimeStampMapper m_timeStamps;

private:
    bool shouldDrop(const ffmpeg::Frame * frame) const;
    std::shared_ptr<ffmpeg::Packet> transcodeVideo(const ffmpeg::Frame * frame, int * outNxError);
    int encode(const ffmpeg::Frame* frame, ffmpeg::Packet * outPacket);

    void waitForTimeSpan(uint64_t msec);
    std::shared_ptr<ffmpeg::Packet> nextPacket(int * outNxError);

    bool ensureInitialized();
    void ensureConsumerAdded() override;
    int initialize();
    void uninitialize();
    int openVideoEncoder();
    int initializeScaledFrame(const ffmpeg::Codec* encoder);
    void setEncoderOptions(ffmpeg::Codec* encoder);
    void maybeDrop();
    int scale(const AVFrame * frame, AVFrame* outFrame);
    void calculateTimePerFrame();
    int framesToDrop();
};

} // namespace usb_cam
} // namespace nx
