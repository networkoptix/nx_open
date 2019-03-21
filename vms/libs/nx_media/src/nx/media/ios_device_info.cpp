#include "ios_device_info.h"

#if defined(Q_OS_IOS)

#include <sys/utsname.h>
#include <QtCore/QRegExp>

#endif // if defined (Q_OS_IOS)

namespace nx::media {

bool IosDeviceInformation::operator==(const IosDeviceInformation& other) const
{
    return type == other.type
        && (majorVersion == 0 || other.majorVersion == 0 || majorVersion == other.majorVersion)
        && (minorVersion == 0 || other.minorVersion == 0 || minorVersion == other.minorVersion);
}

IosDeviceInformation IosDeviceInformation::currentInformation()
{
    #if defined(Q_OS_IOS)
        struct utsname systemInfo;
        uname(&systemInfo);

        const auto& infoString = QString::fromLatin1(systemInfo.machine);

        QRegExp infoRegExp("([^\\d]+)(\\d+),(\\d+)");

        if (!infoRegExp.exactMatch(infoString))
            return IosDeviceInformation();

        IosDeviceInformation info;

        const auto& type = infoRegExp.cap(1);
        if (type == "iPhone")
            info.type = IosDeviceInformation::Type::iPhone;
        else if (type == "iPad")
            info.type = IosDeviceInformation::Type::iPad;

        info.majorVersion = infoRegExp.cap(2).toInt();
        info.minorVersion = infoRegExp.cap(3).toInt();

        return info;
    #else
        return {};
#endif
}

} // namespace nx::media

