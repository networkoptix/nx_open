// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ios_device_info.h"

#include <QtCore/QList>

#if defined(Q_OS_IOS)

#include <sys/utsname.h>
#include <QtCore/QRegularExpression>

#endif // if defined (Q_OS_IOS)

namespace nx::utils {

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

        const QRegularExpression infoRegExp(
            QRegularExpression::anchoredPattern("([^\\d]+)(\\d+),(\\d+)"));

        const auto match = infoRegExp.match(infoString);

        if (!match.hasMatch())
            return IosDeviceInformation();

        IosDeviceInformation info;

        const auto& type = match.capturedView(1);
        if (type == QLatin1String("iPhone"))
            info.type = IosDeviceInformation::Type::iPhone;
        else if (type == QLatin1String("iPad"))
            info.type = IosDeviceInformation::Type::iPad;

        info.majorVersion = match.capturedView(2).toInt();
        info.minorVersion = match.capturedView(3).toInt();

        return info;
    #else
        return {};
#endif
}

} // namespace nx::media
