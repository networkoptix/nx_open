#include "generic_uncompressed_video_frame.h"

namespace nx::vms::server::analytics {

using namespace nx::sdk::analytics;

bool GenericUncompressedVideoFrame::validatePlane(int plane) const
{
    return NX_ASSERT(plane >= 0 && plane < (int) m_data.size(),
        lm("Requested plane %1 of %2").args(plane, m_data.size()));
}

int GenericUncompressedVideoFrame::dataSize(int plane) const
{
    if (!validatePlane(plane))
        return 0;

    return (int) m_data.at(plane).size();
}

const char* GenericUncompressedVideoFrame::data(int plane) const
{
    if (!validatePlane(plane))
        return nullptr;

    const auto& planeData = m_data.at(plane);
    return planeData.empty() ? nullptr : &planeData.front();
}

int GenericUncompressedVideoFrame::lineSize(int plane) const
{
    if (!validatePlane(plane))
        return 0;

    return m_lineSizes.at(plane);
}

} // namespace nx::vms::server::analytics
