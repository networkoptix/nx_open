#pragma once
#if defined(TARGET_OS_IPHONE)

#include <QtCore/QString>

namespace nx {
namespace media {

struct IosDeviceInformation
{
    enum class Type
    {
        iPhone,
        iPad,
        unknown
    };

    Type type = Type::unknown;
    int majorVersion = 0;
    int minorVersion = 0;
};

IosDeviceInformation iosDeviceInformation();

} // namespace media
} // namespace nx

 #endif // defined(TARGET_OS_IPHONE)
 