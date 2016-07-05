//
// This file is generated. Go to pom.xml.
//
#include <client/client_app_info.h>

#include "app_icons.h"

QString QnClientAppInfo::applicationName()
{
    return QStringLiteral("${product.title}");
}

QString QnClientAppInfo::applicationDisplayName()
{
    return QStringLiteral("${product.display.title}");
}

QString QnClientAppInfo::minilauncherBinaryName()
{
    return QStringLiteral("${minilauncher.binary.name}");
}

QString QnClientAppInfo::applauncherBinaryName()
{
    return QStringLiteral("${applauncher.binary.name}");
}

QString QnClientAppInfo::clientBinaryName()
{
    return QStringLiteral("${client.binary.name}");
}

int QnClientAppInfo::videoWallIconId()
{
    return IDI_ICON_VIDEOWALL;
}