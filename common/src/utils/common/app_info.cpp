#include "app_info.h"

#include <nx/network/app_info.h>
#include <nx/utils/app_info.h>

QnAppInfo::QnAppInfo(QObject* parent):
    QObject(parent)
{
}

QString QnAppInfo::organizationNameForSettings()
{
#ifdef _WIN32
    return organizationName();
#else
    return linuxOrganizationName();
#endif
}

QString QnAppInfo::armBox()
{
    return nx::utils::AppInfo::armBox();
}

QString QnAppInfo::applicationPlatform()
{
    return nx::utils::AppInfo::applicationPlatform();
}

QString QnAppInfo::applicationArch()
{
    return nx::utils::AppInfo::applicationArch();
}

QString QnAppInfo::cloudName()
{
    return nx::network::AppInfo::cloudName();
}

bool QnAppInfo::isArm()
{
    return nx::utils::AppInfo::isArm();
}

bool QnAppInfo::isBpi()
{
    return nx::utils::AppInfo::isBpi();
}

bool QnAppInfo::isNx1()
{
    return nx::utils::AppInfo::isNx1();
}

bool QnAppInfo::isAndroid()
{
    return nx::utils::AppInfo::isAndroid();
}

bool QnAppInfo::isIos()
{
    return nx::utils::AppInfo::isIos();
}

bool QnAppInfo::isMobile()
{
    return nx::utils::AppInfo::isMobile();
}
