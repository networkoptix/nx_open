#include "to_string.h"

namespace nx::sdk {

QString toString(const nx::sdk::IDeviceInfo* deviceInfo)
{
    return lm(
        "Id: % 1, Vendor: %2, Model: %3, Firmware: %4, Name: %5, URL: %6, Channel number: %7, "
        "Shared Id: %8, Logical Id: %9")
        .args(
            deviceInfo->id(),
            deviceInfo->vendor(),
            deviceInfo->model(),
            deviceInfo->firmware(),
            deviceInfo->name(),
            deviceInfo->url(),
            deviceInfo->channelNumber(),
            deviceInfo->sharedId(),
            deviceInfo->logicalId()
        ).toQString();
}

} // namespace nx::sdk
