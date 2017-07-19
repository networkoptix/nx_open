#include "tegra_video.h"

#include "config.h"

TegraVideo* TegraVideo::create(const Params& params)
{
    conf.reload();
    conf.skipNextReload(); //< Each of the methods below calls conf.reload().
    if (conf.disable)
        return createStub(params);
    else
        return createImpl(params);
}
