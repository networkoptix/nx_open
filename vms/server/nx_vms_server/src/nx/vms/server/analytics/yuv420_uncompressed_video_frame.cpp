#include "yuv420_uncompressed_video_frame.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::server::analytics {

using namespace nx::sdk::analytics;

bool Yuv420UncompressedVideoFrame::validatePlane(int plane) const
{
    return NX_ASSERT(plane >= 0 && plane < planeCount(),
        lm("Requested plane %1 of %2").args(plane, planeCount()));
}

int Yuv420UncompressedVideoFrame::dataSize(int plane) const
{
    if (!validatePlane(plane))
        return 0;

    switch (plane)
    {
        case 0:
            // Y plane has full resolution.
            return m_frame->linesize[plane] * m_frame->height;

        case 1:
        case 2:
            // Cb and Cr planes have half resolution.
            return m_frame->linesize[plane] * m_frame->height / 2;

        default:
            return 0;
    }
}

const char* Yuv420UncompressedVideoFrame::data(int plane) const
{
    if (!validatePlane(plane))
        return nullptr;

    return (const char*) m_frame->data[plane];
}

int Yuv420UncompressedVideoFrame::lineSize(int plane) const
{
    if (!validatePlane(plane))
        return 0;

    return m_frame->linesize[plane];
}

} // namespace nx::vms::server::analytics
