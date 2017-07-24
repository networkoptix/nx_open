#include "detection_plugin.h"

#include "config.h"

DetectionPlugin* DetectionPlugin::create(const Params& params)
{
    ini().reload();
    ini().skipNextReload(); //< Each of the methods below calls ini().reload().
    if (ini().disable)
        return createStub(params);
    else
        return createImpl(params);
}
