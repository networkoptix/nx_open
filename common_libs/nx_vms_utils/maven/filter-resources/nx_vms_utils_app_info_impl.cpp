//
// This file is generated. Go to pom.xml.
//
#include <nx/vms/utils/app_info.h>

using namespace nx::vms::utils;

QString AppInfo::nativeUriProtocol()
{
    return QStringLiteral("${uri.protocol}");
}

QString AppInfo::nativeUriProtocolDescription()
{
    return QStringLiteral("${client.display.name}");
}

QString AppInfo::desktopFileName()
{
    return QStringLiteral("${installer.name}.desktop");
}

QString AppInfo::iconFileName()
{
    return QStringLiteral("vmsclient-${customization}.png");
}

QString AppInfo::productNameShort()
{
    return QStringLiteral("${product.name.short}");
}
