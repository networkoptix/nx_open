#pragma once

#include <QtCore/QString>

namespace nx::utils {

struct IosDeviceInformation
{
    enum class Type
    {
        iPhone,
        iPad,
        unknown
    };

    enum Version
    {
        iPhoneXs = 11,
        iPhone6 = 7,
        
        iPadAir2 = 5,
        iPadProA12XBionic = 8
    };

    Type type = Type::unknown;
    int majorVersion = 0;
    int minorVersion = 0;

    bool operator==(const IosDeviceInformation& other) const;

    static IosDeviceInformation currentInformation();
};

IosDeviceInformation iosDeviceInformation();

} // namespace nx::media
