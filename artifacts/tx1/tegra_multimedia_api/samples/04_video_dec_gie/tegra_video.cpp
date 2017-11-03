#include "tegra_video.h"

#include "tegra_video_ini.h"

TegraVideo* TegraVideo::create()
{
    ini().reload();
    if (ini().disable)
        return createStub();
    else
        return createImpl();
}
