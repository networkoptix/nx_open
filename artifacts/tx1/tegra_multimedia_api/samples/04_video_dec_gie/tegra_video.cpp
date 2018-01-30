#include "tegra_video.h"

#include "tegra_video_ini.h"
#include "tegra_video_stub.h"

// ATTENTION: To force stub-only impl, define a macro at compiling: -DTEGRA_VIDEO_STUB_ONLY.

#if defined(TEGRA_VIDEO_STUB_ONLY)

TegraVideo* tegraVideoCreate()
{
    ini().reload();
    return tegraVideoCreateStub();
}

#else // defined(TEGRA_VIDEO_STUB_ONLY)

#include "tegra_video_impl.h"

TegraVideo* tegraVideoCreate()
{
    ini().reload();
    if (ini().disable)
        return tegraVideoCreateStub();
    else
        return tegraVideoCreateImpl();
}

#endif // defined(TEGRA_VIDEO_STUB_ONLY)

void tegraVideoDestroy(TegraVideo* tegraVideo)
{
    delete tegraVideo;
}
