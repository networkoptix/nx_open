// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_license_notifier.h"

#include <api/runtime_info_manager.h>
#include <client/client_runtime_settings.h>
#include <core/resource/user_resource.h>
#include <licensing/license.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/settings/system_specific_local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/license/validator.h>
#include <ui/dialogs/license_notification_dialog.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/synctime.h>

using namespace nx::vms::client::desktop;

namespace {

const qint64 kMsPerDay = 1000ll * 60ll * 60ll * 24ll;

const qint64 kWarningTimes[] = {
    kMsPerDay * 15,
    kMsPerDay * 10,
    kMsPerDay * 5,
    kMsPerDay * 3,
    kMsPerDay * 2,
    kMsPerDay,
    0
};

} // namespace

QnWorkbenchLicenseNotifier::QnWorkbenchLicenseNotifier(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
}

QnWorkbenchLicenseNotifier::~QnWorkbenchLicenseNotifier()
{
}

void QnWorkbenchLicenseNotifier::checkLicenses() const
{
    if (context()->closingDown())
        return;

    if (!qnRuntime->isDesktopMode())
        return;

    if (!accessController()->hasPowerUserPermissions())
        return;

    auto licenseLastWarningTime = systemContext()->localSettings()->licenseLastWarningTime();

    const auto currentTime = qnSyncTime->currentMSecsSinceEpoch();

    QList<QnLicensePtr> licenses;
    auto warn = false;

    auto someLicenseWillBeBlocked = false;
    for (const auto& runtimeInfo: runtimeInfoManager()->items()->getItems())
    {
        if (runtimeInfo.data.prematureLicenseExperationDate)
        {
            someLicenseWillBeBlocked = true;
            break;
        }
    }

    nx::vms::license::Validator validator(systemContext());
    for (const auto& license: systemContext()->licensePool()->getLicenses())
    {
        const auto licenseKey = QString::fromUtf8(license->key());
        const auto errorCode = validator.validate(license);

        if (someLicenseWillBeBlocked
            && errorCode != nx::vms::license::QnLicenseErrorCode::NoError
            && errorCode != nx::vms::license::QnLicenseErrorCode::Expired
            && errorCode != nx::vms::license::QnLicenseErrorCode::TemporaryExpired)
        {
            licenses.push_back(license);
            licenseLastWarningTime[licenseKey] = currentTime;
            warn = true;
            continue;
        }

        const auto expirationTime = license->expirationTime();
        // skip infinite license
        if (expirationTime < 0)
            continue;

        const auto licenseWarningTime = licenseLastWarningTime[licenseKey];
        // skip already notified expired license
        const auto lastTimeLeft = expirationTime - licenseWarningTime;
        if (lastTimeLeft < 0)
            continue;
        // skip license that didn't pass the maximum pre-notification interval
        const auto timeLeft = expirationTime - currentTime;
        if (timeLeft > kWarningTimes[0])
            continue;

        licenses.push_back(license);

        auto lastWarningIndex = 0;
        for (; kWarningTimes[lastWarningIndex] > lastTimeLeft; lastWarningIndex++);

        auto warningIndex = 0;
        for (; kWarningTimes[warningIndex] > timeLeft
            && kWarningTimes[warningIndex] > 0; warningIndex++);

        // warn user only if pre-notification interval has been changed.
        if (warningIndex == lastWarningIndex && timeLeft > 0)
            continue;

        warn = true;
        licenseLastWarningTime[licenseKey] = currentTime;
    }

    if (warn && mainWindowWidget())
    {
        QScopedPointer<QnLicenseNotificationDialog> dialog(
            new QnLicenseNotificationDialog(mainWindowWidget()));
        dialog->setLicenses(licenses);
        dialog->exec();

        systemContext()->localSettings()->licenseLastWarningTime = licenseLastWarningTime;
    }
}
