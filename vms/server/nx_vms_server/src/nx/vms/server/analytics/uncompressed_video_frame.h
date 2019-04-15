#pragma once

#include <memory>
#include <vector>

#include <utils/media/ffmpeg_helper.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

extern "C" {

#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>

} // extern "C"

namespace nx::vms::server::analytics {

/**
 * Wrapper around ffmpeg's AVFrame.
 */
class UncompressedVideoFrame:
    public nx::sdk::RefCountable<nx::sdk::analytics::IUncompressedVideoFrame>
{
public:
    /**
     * Creates the owning wrapper for AvFrameHolder which may or may not own its AVFrame.
     *
     * On error, the message is logged, an assertion fails and an internal flag is assigned so
     * that the subsequent isValid() returns false.
     *
     * @param avFrame Ffmpeg's frame which should be fully allocated and have one of the supported
     *     pixel formats. ATTENTION: The frame's field pkt_dts is treated as containing timestamp
     *     in microseconds since epoch, contrary to ffmpeg doc (which states it should be in
     *     time_base units).
     */
    explicit UncompressedVideoFrame(std::shared_ptr<const AvFrameHolder> avFrame);

    /**
     * Whether the frame was successfully constructed. If this method returns false, no other
     * methods should be called except the destructor.
     */
    bool isValid() const { return m_valid; }

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
    bool assertValid(const char* func) const;
    bool assertPlaneValid(int plane, const char* func) const;
    int planeLineCount(int plane) const;

private:
    bool m_valid = false;
    const std::shared_ptr<const AvFrameHolder> m_avFrame;
    PixelFormat m_pixelFormat;
    const AVPixFmtDescriptor* m_avPixFmtDescriptor;
    std::vector<int> m_dataSize;
};

} // namespace nx::vms::server::analytics
