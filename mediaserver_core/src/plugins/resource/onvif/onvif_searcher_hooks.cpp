#include "onvif_searcher_hooks.h"

namespace nx {
namespace plugins {
namespace onvif {
namespace searcher_hooks {

void hikvisionManufacturerReplacement(EndpointAdditionalInfo* outInfo)
{
    const auto kHikvisionManufacturer = lit("Hikvision");
    auto currentManufacturer = outInfo->manufacturer.toLower();
    auto currentName = outInfo->name.toLower();

    bool applyHikvisonManufacturerHook =
        currentName.startsWith(kHikvisionManufacturer.toLower())
        && currentName.endsWith(currentManufacturer);

    if (applyHikvisonManufacturerHook)
    {
        outInfo->manufacturer = kHikvisionManufacturer;
        auto split = outInfo->name.split(L' ');
        if (split.size() == 2)
            outInfo->name = split[1];
        else
            outInfo->name = outInfo->name.mid(kHikvisionManufacturer.size());
    }
}


} // namespace searcher_hooks
} // namespace onvif
} // namespace plugins
} // namespace nx