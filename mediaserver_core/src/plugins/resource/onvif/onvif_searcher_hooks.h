#pragma once
#if defined(ENABLE_ONVIF)

#include "onvif_resource_information_fetcher.h"

namespace nx {
namespace plugins {
namespace onvif {
namespace searcher_hooks {

void commonHooks(EndpointAdditionalInfo* outInfo);

void hikvisionManufacturerReplacement(EndpointAdditionalInfo* outInfo);

void manufacturerReplacementByModel(EndpointAdditionalInfo* outInfo);

void pelcoModelNormalization(EndpointAdditionalInfo* outInfo);

} // namespace searcher_hooks
} // namespace onvif
} // namespace plugins
} // namespace nx

#endif // defined(ENABLE_ONVIF)
