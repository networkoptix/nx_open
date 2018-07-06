#include "workbench_license_notifier.h"

#include <api/runtime_info_manager.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource/user_resource.h>

#include <licensing/license.h>
#include <licensing/license_validator.h>

#include <ui/dialogs/license_notification_dialog.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/synctime.h>

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

static const int kMessagesDelayMs = 5000;

} // anonymous namespace


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

    if (!accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
        return;

    auto licenseWarningStates = qnSettings->licenseWarningStates();

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

    for (const auto& license: licensePool()->getLicenses())
    {
        const auto errorCode = licensePool()->validateLicense(license);

        if (someLicenseWillBeBlocked
            && errorCode != QnLicenseErrorCode::NoError
            && errorCode != QnLicenseErrorCode::Expired)
        {
            licenses.push_back(license);
            licenseWarningStates[license->key()].lastWarningTime = currentTime;
            warn = true;
            continue;
        }

        const auto expirationTime = license->expirationTime();
        // skip infinite license
        if (expirationTime < 0)
            continue;

        const auto licenseWarningState = licenseWarningStates.value(license->key());
        // skip already notified expired license
        const auto lastTimeLeft = expirationTime - licenseWarningState.lastWarningTime;
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
        licenseWarningStates[license->key()].lastWarningTime = currentTime;
    }

    if (warn && mainWindowWidget())
    {
        QScopedPointer<QnLicenseNotificationDialog> dialog(
            new QnLicenseNotificationDialog(mainWindowWidget()));
        dialog->setLicenses(licenses);
        dialog->exec();

        qnSettings->setLicenseWarningStates(licenseWarningStates);
    }
}
