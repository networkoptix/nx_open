#pragma once

#include "stream_reader.h"

#include <map>
#include <deque>
#include <thread>

#include "camera/buffered_stream_consumer.h"
#include "camera/timestamp_mapper.h"

namespace nx { namespace usb_cam { namespace ffmpeg { class Codec; } } }
namespace nx { namespace usb_cam { namespace ffmpeg { class Frame; } } }

struct SwsContext;

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
    struct PictureParameters
    {
        int width;
        int height;
        AVPixelFormat pixelFormat;

        PictureParameters(
            int width = -1,
            int height = -1,
            AVPixelFormat pixelFormat = AV_PIX_FMT_NONE)
            :
            width(width),
            height(height),
            pixelFormat(pixelFormat)
        {
        }

        PictureParameters(const AVFrame * frame):
            width(frame->width),
            height(frame->height),
            pixelFormat((AVPixelFormat)frame->format)
        {
        }

        bool operator==(const PictureParameters& rhs) const
        {
            return width == rhs.width && height == rhs.height && pixelFormat == rhs.pixelFormat;
        }

        bool operator!=(const PictureParameters& rhs) const
        {
            return !operator==(rhs);
        }
    };

    struct ScaleParameters
    {
        PictureParameters input;
        PictureParameters output;

        ScaleParameters(
            const PictureParameters& input = PictureParameters(),
            const PictureParameters& output = PictureParameters())
            :
            input(input),
            output(output)
        {}

        bool operator==(const ScaleParameters& rhs) const
        {
            return input == rhs.input && output == rhs.output;
        }

        bool operator!=(const ScaleParameters& rhs) const
        {
            return !operator==(rhs);
        }
    };

private:
    CodecParameters m_codecParams;
    std::shared_ptr<BufferedVideoFrameConsumer> m_videoFrameConsumer;

    int m_initCode = 0;

    std::unique_ptr<ffmpeg::Codec> m_encoder;
    bool m_encoderNeedsReinitialization = true;

    ScaleParameters m_scaleParams;
    SwsContext * m_scaleContext = nullptr;
    std::unique_ptr<ffmpeg::Frame> m_scaledFrame;

    TimestampMapper m_timestamps;

    uint64_t m_lastVideoPts = 0;
    uint64_t m_lastTimestamp = 0;
    uint64_t m_timePerFrame = 0;

private:
    bool shouldDrop(const ffmpeg::Frame * frame);
    void transcode(const std::shared_ptr<ffmpeg::Frame>& frame);
    std::shared_ptr<ffmpeg::Packet> transcodeVideo(const ffmpeg::Frame * frame, int * outNxError);
    int encode(const ffmpeg::Frame* frame, ffmpeg::Packet * outPacket);

    bool waitForTimespan(
        const std::chrono::milliseconds& timespan,
        const std::chrono::milliseconds& timeout);

    std::shared_ptr<ffmpeg::Packet> nextPacket(int * outNxError);

    bool ensureEncoderInitialized();
    void ensureConsumerAdded() override;
    void uninitialize();
    int initializeVideoEncoder();
    int initializeScaledFrame(const ffmpeg::Codec* encoder);
    int initializeScaledFrame(const PictureParameters& pictureParams);
    void setEncoderOptions(ffmpeg::Codec* encoder);

    int initializeScaleContext(const ScaleParameters & scaleParams);
    void uninitializeScaleContext();
    int reinitializeScaleContext(const ScaleParameters& scaleParams);
    ScaleParameters getNewestScaleParameters(const AVFrame * frame) const;
    
    /**
     * Scale @param frame, modifying the preallocated @param outFrame whose size and format are
     * expected to have already been set.
     * 
     * @param[in] frame - the input frame
     * @return Ffmpeg error code, 0 on success negative value on failure
     */
    int scale(const AVFrame * frame);
    void calculateTimePerFrame();

    virtual void removeVideoConsumer() override;
};

} // namespace usb_cam
} // namespace nx
