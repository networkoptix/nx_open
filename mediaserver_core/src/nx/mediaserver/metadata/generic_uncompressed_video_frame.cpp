#include "generic_uncompressed_video_frame.h"

namespace nx {
namespace sdk {
namespace metadata {

void* GenericUncompressedVideoFrame::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_UncompressedVideoFrame)
    {
        addRef();
        return static_cast<UncompressedVideoFrame*>(this);
    }
    if (interfaceId == IID_MediaFrame)
    {
        addRef();
        return static_cast<MediaFrame*>(this);
    }
    if (interfaceId == IID_DataPacket)
    {
        addRef();
        return static_cast<DataPacket*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

bool GenericUncompressedVideoFrame::validatePlane(int plane) const
{
    if (plane >= m_data.size())
    {
        NX_ASSERT(false, lm("Requested plane %1 of %2").args(plane, m_data.size()));
        return false;
    }
    return true;
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

} // namespace metadata
} // namespace sdk
} // namespace nx
