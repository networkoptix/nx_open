#pragma once

// FFmpeg headers
extern "C" {
#include <libavutil/frame.h>
}

#include <chrono>
#include <memory>

#include <mfx/mfxvideo.h>

#include "allocators/sysmem_allocator.h"

class SurfacePool
{
public:
    struct Config
    {
        std::chrono::milliseconds surfaceWaitTimeout {3000};
    };

public:
    SurfacePool(const Config& config)
        : m_config(config)
    {
    }
    ~SurfacePool()
    {
        delete [] m_surfaces;
    }
    void setAllocator(std::unique_ptr<MFXFrameAllocator> allocator);
    bool alloc(mfxFrameAllocRequest& request, const mfxFrameInfo& frameInfo);
    mfxFrameSurface1* getSurface();

private:
    Config m_config;
    mfxFrameAllocResponse m_response;
    mfxFrameSurface1* m_surfaces {nullptr};
    std::unique_ptr<MFXFrameAllocator> m_mfxAllocator;
    std::unique_ptr<mfxAllocatorParams> m_pmfxAllocatorParams;
};

