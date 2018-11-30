#pragma once

#include "stream_reader.h"

#include <map>
#include <deque>
#include <thread>

#include "camera/buffered_stream_consumer.h"
#include "camera/timestamp_mapper.h"

namespace nx { namespace usb_cam { namespace ffmpeg { class Codec; } } }
namespace nx { namespace usb_cam { namespace ffmpeg { class Frame; } } }

namespace nx {
namespace usb_cam {

class TranscodeStreamReader: public StreamReaderPrivate
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
    StreamState m_cameraState = csOff;

    int m_retries = 0;
    int m_initCode = 0;

    uint64_t m_lastVideoPts = 0;
    uint64_t m_lastTimestamp = 0;
    uint64_t m_timePerFrame = 0;
    std::shared_ptr<BufferedVideoFrameConsumer> m_videoFrameConsumer;
    
    std::unique_ptr<ffmpeg::Codec> m_encoder;
    std::unique_ptr<ffmpeg::Frame> m_scaledFrame;

    TimestampMapper m_timestamps;

private:
    bool shouldDrop(const ffmpeg::Frame * frame);
    void transcode(const std::shared_ptr<ffmpeg::Frame>& frame);
    std::shared_ptr<ffmpeg::Packet> transcodeVideo(const ffmpeg::Frame * frame, int * outNxError);
    int encode(const ffmpeg::Frame* frame, ffmpeg::Packet * outPacket);

    bool waitForTimeSpan(
    const std::chrono::milliseconds& timeSpan,
    const std::chrono::milliseconds& timeout);
    std::shared_ptr<ffmpeg::Packet> nextPacket(int * outNxError);

    bool ensureInitialized();
    void ensureConsumerAdded() override;
    int initialize();
    void uninitialize();
    int openVideoEncoder();
    int initializeScaledFrame(const ffmpeg::Codec* encoder);
    void setEncoderOptions(ffmpeg::Codec* encoder);
    
    /**
     * Scale @param frame, modifying the preallocated @param outFrame whose size and format are
     * expected to have already been set.
     * 
     * @param[in] frame - the input frame
     * @params[out] outFrame - the output frame
     * @return Ffmpeg error code, 0 on success negative value on failure
     */
    int scale(const AVFrame * frame, AVFrame* outFrame);
    void calculateTimePerFrame();

    virtual void removeVideoConsumer() override;
};

} // namespace usb_cam
} // namespace nx
