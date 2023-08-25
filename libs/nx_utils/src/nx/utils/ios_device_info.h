// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::utils {

struct NX_UTILS_API IosDeviceInformation
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

    static IosDeviceInformation currentInformation();
};

IosDeviceInformation iosDeviceInformation();

} // namespace nx::media
