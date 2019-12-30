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

/**
 * Some Vivotek cameras in the response for a WS-discovery request declare something like the
 * following in the Scopes of ProbeMatch:
 * ```
 * <Scopes>
 *      onvif://www.onvif.org/hardware/FD9187-HT
 *      onvif://www.onvif.org/name/VIVOTEK%20FD9187-HT
 *      ...
 * </Scopes>
 * ```
 * Because of that we have "VIVOTEK%20FD9187-HT" as a model name and "FD9187-HT" as a vendor name.
 * The hook corrects this situation.
 */
void vivotekHook(QnResourceDataPool* dataPool, EndpointAdditionalInfo* outInfo);

} // namespace searcher_hooks
} // namespace onvif
} // namespace plugins
} // namespace nx

#endif // defined(ENABLE_ONVIF)
