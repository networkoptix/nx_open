#include "mobile_app_info.h"

#include <nx/network/app_info.h>
#include <nx/utils/software_version.h>
#include <utils/common/app_info.h>
#include <mobile_client/mobile_client_app_info.h>
#include <nx/utils/app_info.h>

QnMobileAppInfo::QnMobileAppInfo(QObject* parent)
    : QObject(parent)
{

}

QString QnMobileAppInfo::productName() const
{
    return QnAppInfo::productNameLong();
}

QString QnMobileAppInfo::organizationName() const
{
    return QnAppInfo::organizationName();
}

QString QnMobileAppInfo::version() const
{
    return lit("%1.%2 (rev: %3)").arg(
        QnMobileClientAppInfo::applicationVersion(),
        QString::number(nx::utils::SoftwareVersion(QnAppInfo::applicationVersion()).build()),
        QnAppInfo::applicationRevision());
}

QString QnMobileAppInfo::cloudName() const
{
    return nx::network::AppInfo::cloudName();
}

QString QnMobileAppInfo::liteDeviceName() const
{
    return QnMobileClientAppInfo::liteDeviceName();
}

QUrl QnMobileAppInfo::oldMobileClientUrl() const
{
#if defined(Q_OS_ANDROID)
    return QnMobileClientAppInfo::oldAndroidClientLink();
#elif defined(Q_OS_IOS)
    return QnMobileClientAppInfo::oldIosClientLink();
#else
    return QUrl();
#endif
}
