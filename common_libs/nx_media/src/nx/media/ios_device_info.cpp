#include "ios_device_info.h"

#if defined (Q_OS_IOS)

#include <sys/utsname.h>

#include <QtCore/QRegExp>

namespace nx {
namespace media {

IosDeviceInformation iosDeviceInformation()
{
    struct utsname systemInfo;
    uname(&systemInfo);

    const auto& infoString = QString::fromLatin1(systemInfo.machine);

    QRegExp infoRegExp(lit("(\\w+)(\\d+),(\\d+)"));

    if (!infoRegExp.exactMatch(infoString))
        return IosDeviceInformation();

    IosDeviceInformation info;

    const auto& type = infoRegExp.cap(1);
    if (type == lit("iPhone"))
        info.type = IosDeviceInformation::Type::iPhone;
    else if (type == lit("iPad"))
        info.type = IosDeviceInformation::Type::iPad;

    info.majorVersion = infoRegExp.cap(2).toInt();
    info.minorVersion = infoRegExp.cap(3).toInt();

    return info;
}

} // namespace media
} // namespace nx

#endif // if defined (Q_OS_IOS)
