#pragma once

#include <QtCore/QString>

#if defined (Q_OS_IOS)

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

#endif // if defined (Q_OS_IOS)
