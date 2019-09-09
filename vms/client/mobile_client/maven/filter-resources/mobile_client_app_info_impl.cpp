//
// This file is generated. Go to pom.xml.
//
#include <mobile_client/mobile_client_app_info.h>

QString QnMobileClientAppInfo::applicationName()
{
    return QStringLiteral("${mobile_client.name}");
}

QString QnMobileClientAppInfo::applicationDisplayName()
{
    return QStringLiteral("${mobile_client.display.name}");
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
    return QStringLiteral("https://play.google.com/store/apps/details?id=${customization.mobile.android.compatibilityPackage}");
}

QString QnMobileClientAppInfo::oldIosClientLink()
{
    return QStringLiteral("https://itunes.apple.com/app/${ios.old_app_appstore_id}");
}

QString QnMobileClientAppInfo::oldAndroidAppId()
{
    return QStringLiteral("${customization.mobile.android.compatibilityPackage}");
}

QString QnMobileClientAppInfo::liteDeviceName()
{
    return QStringLiteral("${liteDeviceName}");
}
