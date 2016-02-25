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

int QnClientAppInfo::videoWallIconId()
{
    return IDI_ICON_VIDEOWALL;
}