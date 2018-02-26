#include "common_uncompressed_video_frame.h"

#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace metadata {

void* CommonUncompressedVideoFrame::queryInterface(const nxpl::NX_GUID& interfaceId)
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

int CommonUncompressedVideoFrame::planeCount() const
{
    if (!m_externalData.empty())
        return (int) m_externalData.size();
    if (!m_ownedData.empty())
        return (int) m_ownedData.size();
    return 0;
}

bool CommonUncompressedVideoFrame::checkPlane(int plane, const char* func) const
{
    const int count = planeCount();
    if (plane >= count)
    {
        NX_PRINT << "ERROR: " << func << "(): Requested plane " << plane << " of " << count;
        return false;
    }

    return true;
}

int CommonUncompressedVideoFrame::dataSize(int plane) const
{
    if (!checkPlane(plane, __func__))
        return 0;

    if (!m_externalData.empty())
        return m_externalDataSize.at(plane);
    if (!m_ownedData.empty())
        return m_ownedData.at(plane).size();

    return 0;
}

const char* CommonUncompressedVideoFrame::data(int plane) const
{
    if (!checkPlane(plane, __func__))
        return nullptr;

    if (!m_externalData.empty())
        return m_externalData.at(plane);

    if (!m_ownedData.empty())
    {
        const auto& planeData = m_ownedData.at(plane);
        return planeData.empty() ? nullptr : &planeData.front();
    }

    return nullptr;
}

int CommonUncompressedVideoFrame::lineSize(int plane) const
{
    if (!checkPlane(plane, __func__))
        return 0;

    return m_lineSize.at(plane);
}

} // namespace metadata
} // namespace sdk
} // namespace nx
