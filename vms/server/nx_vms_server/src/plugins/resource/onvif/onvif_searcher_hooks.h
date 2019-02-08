#pragma once
#if defined(ENABLE_ONVIF)

#include "onvif_resource_information_fetcher.h"

namespace nx {
namespace plugins {
namespace onvif {
namespace searcher_hooks {

void commonHooks(QnResourceDataPool* dataPool, EndpointAdditionalInfo* outInfo);

void hikvisionManufacturerReplacement(
    QnResourceDataPool* dataPool, EndpointAdditionalInfo* outInfo);

void manufacturerReplacementByModel(
    QnResourceDataPool* dataPool, EndpointAdditionalInfo* outInfo);

void pelcoModelNormalization(
    QnResourceDataPool* dataPool, EndpointAdditionalInfo* outInfo);

void forcedAdditionalManufacturer(
    QnResourceDataPool* dataPool, EndpointAdditionalInfo* outInfo);

void additionalManufacturerNormalization(
    QnResourceDataPool* dataPool, EndpointAdditionalInfo* outInfo);

void swapVendorAndModel(QnResourceDataPool* dataPool, EndpointAdditionalInfo* outInfo);

} // namespace searcher_hooks
} // namespace onvif
} // namespace plugins
} // namespace nx

#endif // defined(ENABLE_ONVIF)
