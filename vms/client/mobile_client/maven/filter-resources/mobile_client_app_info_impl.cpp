#include <mobile_client/mobile_client_app_info.h>

QString QnMobileClientAppInfo::applicationName()
{
    //TODO: #GDM Customize as mobile client internal name to store settings?
    return QStringLiteral("${customization.companyName} ${product.name} Mobile Client");
}

QString QnMobileClientAppInfo::applicationDisplayName()
{
    return QStringLiteral("${customization.mobile.displayName}");
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
    return QStringLiteral("https://play.google.com/store/apps/details?id=") + oldAndroidAppId();
}

QString QnMobileClientAppInfo::oldIosClientLink()
{
    return QStringLiteral("https://itunes.apple.com/app/${customization.mobile.ios.compatibilityPackage}");
}

QString QnMobileClientAppInfo::oldAndroidAppId()
{
    return QStringLiteral("${customization.mobile.android.compatibilityPackage}");
}

QString QnMobileClientAppInfo::liteDeviceName()
{
    return QStringLiteral("${liteDeviceName}");
}
