#include "to_string.h"

namespace nx::sdk {

QString toString(const nx::sdk::DeviceInfo& deviceInfo)
{
    return lm(
        "Vendor: %1, Model: %2, Firmware: %3, UID: %4, Shared ID: %5, URL: %6, Channel: %7")
        .args(
            deviceInfo.vendor,
            deviceInfo.model,
            deviceInfo.firmware,
            deviceInfo.uid,
            deviceInfo.sharedId,
            deviceInfo.url,
            deviceInfo.channel
        ).toQString();
}

} // namespace nx::sdk
