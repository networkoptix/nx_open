#pragma once

#if defined (Q_OS_IOS)

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

#endif // if defined (Q_OS_IOS)
