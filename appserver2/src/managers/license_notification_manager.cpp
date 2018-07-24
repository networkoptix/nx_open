#include "license_notification_manager.h"

#include <nx_ec/data/api_conversion_functions.h>

using namespace nx::vms::api;

namespace ec2 {

void QnLicenseNotificationManager::triggerNotification(
    const QnTransaction<LicenseDataList>& tran, NotificationSource /*source*/)
{
    QnLicenseList licenseList;
    fromApiToResourceList(tran.params, licenseList);

    for (const QnLicensePtr& license : licenseList) {
        emit licenseChanged(license);
    }
}

void QnLicenseNotificationManager::triggerNotification(
    const QnTransaction<LicenseData>& tran, NotificationSource /*source*/)
{
    QnLicensePtr license(new QnLicense());
    fromApiToResource(tran.params, license);
    if (tran.command == ApiCommand::addLicense)
        emit licenseChanged(license);
    else if (tran.command == ApiCommand::removeLicense)
        emit licenseRemoved(license);
}

} // namespace ec2
