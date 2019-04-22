#pragma once

#include <memory>
#include <vector>

#include <utils/media/frame_info.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

extern "C" {

#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>

} // extern "C"

namespace nx::vms::server::analytics {

/**
 * Wrapper around ffmpeg's AVFrame, either owning an instance or holding a reference to the shared
 * instance (depending on which constructor is used).
 *
 * ATTENTION: The frame's field pkt_dts is treated as containing timestamp in microseconds since
 * epoch, contrary to ffmpeg doc (which states it should be in time_base units).
 */
class UncompressedVideoFrame:
    public nx::sdk::RefCountable<nx::sdk::analytics::IUncompressedVideoFrame>
{
public:
    /**
     * Creates the wrapper for a shared instance of CLVideoDecoderOutput (inherits AVFrame). On
     * error, an assertion fails and the subsequent calls to avFrame() return null.
     *
     * @param clVideoDecoderOutput An existing AVFrame which is allocated and have one of the
     *     supported pixel formats.
     */
    explicit UncompressedVideoFrame(CLConstVideoDecoderOutputPtr clVideoDecoderOutput);

    /**
     * Creates and owns an instance of AVFrame, allocating its buffers and setting its fields. On
     * error, an assertion fails and the subsequent calls to avFrame() return null.
     */
    explicit UncompressedVideoFrame(int width, int height, AVPixelFormat pixelFormat, int64_t dts);

    /**
     * @return The frame, either owned or referenced, and null in case an assertion has failed
     *     in the constructor (if so, no other methods should be called except the destructor).
     */
    const AVFrame* avFrame() const { return m_avFrame; }

    virtual int64_t timestampUs() const override;
    virtual int width() const override;
    virtual int height() const override;
    virtual PixelAspectRatio pixelAspectRatio() const override;
    virtual PixelFormat pixelFormat() const override;
    virtual int planeCount() const override;
    virtual int dataSize(int plane) const override;
    virtual const char* data(int plane) const override;
    virtual int lineSize(int plane) const override;

private:
    void acceptAvFrame(const AVFrame* avFrame);
    bool assertValid(const char* func) const;
    bool assertPlaneValid(int plane, const char* func) const;

private:
    const CLConstVideoDecoderOutputPtr m_clVideoDecoderOutput; //< Used only if no m_ownedAvFrame.
    std::shared_ptr<AVFrame> m_ownedAvFrame; //< Used only if no m_clVideoDecoderOutput.
    const AVFrame* m_avFrame = nullptr; //< Either m_ownedAvFrame or m_clVideoDecoderOutput.

    PixelFormat m_pixelFormat;
    const AVPixFmtDescriptor* m_avPixFmtDescriptor;
    std::vector<int> m_dataSize;
};

} // namespace nx::vms::server::analytics
