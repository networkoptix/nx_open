#include "generic_uncompressed_video_frame.h"

namespace nx::vms::server::analytics {

using namespace nx::sdk::analytics;

void* GenericUncompressedVideoFrame::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_UncompressedVideoFrame)
    {
        addRef();
        return static_cast<IUncompressedVideoFrame*>(this);
    }
    if (interfaceId == IID_UncompressedMediaFrame)
    {
        addRef();
        return static_cast<IUncompressedMediaFrame*>(this);
    }
    if (interfaceId == IID_DataPacket)
    {
        addRef();
        return static_cast<IDataPacket*>(this);
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
    if (plane >= (int) m_data.size())
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

} // namespace nx::vms::server::analytics
