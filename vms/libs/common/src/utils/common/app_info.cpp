#include "app_info.h"

#include <nx/network/app_info.h>
#include <nx/utils/app_info.h>

QnAppInfo::QnAppInfo(QObject* parent):
    QObject(parent)
{
}

QString QnAppInfo::organizationName()
{
    return nx::utils::AppInfo::organizationName();
}

QString QnAppInfo::linuxOrganizationName()
{
    return nx::utils::AppInfo::linuxOrganizationName();
}

QString QnAppInfo::applicationVersion()
{
    return nx::utils::AppInfo::applicationVersion();
}

QString QnAppInfo::applicationRevision()
{
    return nx::utils::AppInfo::applicationRevision();
}

QString QnAppInfo::armBox()
{
    return nx::utils::AppInfo::armBox();
}

bool QnAppInfo::beta()
{
    return nx::utils::AppInfo::beta();
}

QString QnAppInfo::productNameShort()
{
    return nx::utils::AppInfo::productNameShort();
}

QString QnAppInfo::productNameLong()
{
    return nx::utils::AppInfo::productNameLong();
}

QString QnAppInfo::customizationName()
{
    return nx::utils::AppInfo::customizationName();
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

bool QnAppInfo::isWindows()
{
    return nx::utils::AppInfo::isWindows();
}

nx::vms::api::SystemInformation QnAppInfo::currentSystemInformation()
{
    return nx::vms::api::SystemInformation(
        applicationPlatform(), applicationArch(), applicationPlatformModification());
}
