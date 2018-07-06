#include "license_notification_manager.h"
#include "nx_ec/data/api_conversion_functions.h"

namespace ec2 {

void QnLicenseNotificationManager::triggerNotification(const QnTransaction<ApiLicenseDataList>& tran, NotificationSource /*source*/)
{
    QnLicenseList licenseList;
    fromApiToResourceList(tran.params, licenseList);

    for (const QnLicensePtr& license : licenseList) {
        emit licenseChanged(license);
    }
}

void QnLicenseNotificationManager::triggerNotification(const QnTransaction<ApiLicenseData>& tran, NotificationSource /*source*/)
{
    QnLicensePtr license(new QnLicense());
    fromApiToResource(tran.params, license);
    if (tran.command == ApiCommand::addLicense)
        emit licenseChanged(license);
    else if (tran.command == ApiCommand::removeLicense)
        emit licenseRemoved(license);
}

} // namespace ec2
