//
// This file is generated. Go to pom.xml.
//
#include <mobile_client/mobile_client_app_info.h>

QString QnMobileClientAppInfo::applicationName()
{
    return QStringLiteral("${product.title}");
}

QString QnMobileClientAppInfo::applicationDisplayName()
{
    return QStringLiteral("${product.display.title}");
}

bool QnMobileClientAppInfo::defaultLiteMode()
{
    return ${liteMode};
}
