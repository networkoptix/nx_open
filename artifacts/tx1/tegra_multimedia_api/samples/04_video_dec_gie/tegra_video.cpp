#include "tegra_video.h"

// ATTENTION: To force stub-only impl, define a macro at compiling: -DTEGRA_VIDEO_STUB_ONLY.

#include "tegra_video_ini.h"

TegraVideo* TegraVideo::create()
{
    ini().reload();
    if (ini().disable)
        return createStub();
    else
        return createImpl();
}

#if defined(TEGRA_VIDEO_STUB_ONLY)

TegraVideo* TegraVideo::createImpl()
{
    return createStub();
}

#endif // defined(TEGRA_VIDEO_STUB_ONLY)
