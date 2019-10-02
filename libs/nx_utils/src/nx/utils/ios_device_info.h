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
        iPhone6 = 7,        
        iPadAir2 = 5
    };

    Type type = Type::unknown;
    int majorVersion = 0;
    int minorVersion = 0;

    bool operator==(const IosDeviceInformation& other) const;

    bool isBionicProcessor() const;

    static IosDeviceInformation currentInformation();
};

IosDeviceInformation iosDeviceInformation();

} // namespace nx::media
