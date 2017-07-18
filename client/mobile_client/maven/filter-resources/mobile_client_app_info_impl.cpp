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

QString QnMobileClientAppInfo::applicationVersion()
{
    return QStringLiteral("${mobileClientVersion}");
}

bool QnMobileClientAppInfo::defaultLiteMode()
{
    return ${liteMode};
}

QString QnMobileClientAppInfo::oldAndroidClientLink()
{
    return QStringLiteral("https://play.google.com/store/apps/details?id=${android.oldPackageName}");
}

QString QnMobileClientAppInfo::oldIosClientLink()
{
    return QStringLiteral("https://itunes.apple.com/app/${ios.old_app_appstore_id}");
}

QString QnMobileClientAppInfo::oldAndroidAppId()
{
    return QStringLiteral("${android.oldPackageName}");
}

QString QnMobileClientAppInfo::liteDeviceName()
{
    return QStringLiteral("${liteDeviceName}");
}
