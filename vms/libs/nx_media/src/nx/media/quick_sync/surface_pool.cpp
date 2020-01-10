#include "surface_pool.h"

#include <thread>

#include <nx/utils/log/log.h>

mfxFrameSurface1* SurfacePool::getSurface()
{
    const std::chrono::milliseconds sleepInterval {10};
    std::chrono::milliseconds waitTime {0};
    while (waitTime < m_config.surfaceWaitTimeout)
    {
        for (mfxU16 i = 0; i < m_response.NumFrameActual; i++)
        {
            if (0 == m_surfaces[i].Data.Locked)
            {
                m_surfaces[i].Info.FrameId.ViewId = 0;
                return &m_surfaces[i];
            }
        }
        std::this_thread::sleep_for(sleepInterval);
        waitTime += sleepInterval;
    }
    NX_ERROR(this, "Failed to get surface, timeout expired: %1msec", m_config.surfaceWaitTimeout);
    return nullptr;
}

void SurfacePool::setAllocator(std::unique_ptr<MFXFrameAllocator> allocator)
{
    m_mfxAllocator = std::move(allocator);
}

bool SurfacePool::alloc(mfxFrameAllocRequest& request, const mfxFrameInfo& frameInfo)
{
    if (!m_mfxAllocator)
    {
        NX_ERROR(this, "Alloc failed: empty allocator");
        return false;
    }

    mfxStatus status = m_mfxAllocator->Alloc(m_mfxAllocator->pthis, &request, &m_response);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "Alloc failed, status: %1", status);
        return false;
    }
    m_surfaces = new mfxFrameSurface1[m_response.NumFrameActual];
    for (int i = 0; i < m_response.NumFrameActual; i++)
    {
        memset(&m_surfaces[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&m_surfaces[i].Info, &frameInfo, sizeof(frameInfo));
        m_surfaces[i].Data.MemType = request.Type;

        // get YUV pointers
//         status = m_mfxAllocator->Lock(
//             m_mfxAllocator->pthis, m_response.mids[i], &(m_surfaces[i].Data));
//         if (status < MFX_ERR_NONE)
//         {
//             NX_ERROR(this, "Lock failed, status: %1", status);
//             return false;
//         }
    }
    return true;
}
