#pragma once

namespace nx {
namespace sdk {

// TODO: Split into IDeviceInfo and DeviceInfo.
struct DeviceInfo
{
    DeviceInfo()
    {
        vendor[0] = 0;
        model[0] = 0;
        firmware[0] = 0;
        uid[0] = 0;
        sharedId[0] = 0;
        url[0] = 0;
        login[0] = 0;
        password[0] = 0;
    }

    static const int kStringParameterMaxLength = 256;
    static const int kTextParameterMaxLength = 1024;

    char vendor[kStringParameterMaxLength];
    char model[kStringParameterMaxLength];
    char firmware[kStringParameterMaxLength];
    char uid[kStringParameterMaxLength];
    char sharedId[kStringParameterMaxLength];
    char url[kTextParameterMaxLength];
    char login[kStringParameterMaxLength];
    char password[kStringParameterMaxLength];
    int channel = 0;
    int logicalId = 0;
};

} // namespace sdk
} // namespace nx
