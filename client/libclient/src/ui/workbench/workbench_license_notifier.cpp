#include "workbench_license_notifier.h"

#include <api/runtime_info_manager.h>

#include <client/client_settings.h>

#include <core/resource/user_resource.h>

#include <licensing/license.h>

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

} // anonymous namespace


QnWorkbenchLicenseNotifier::QnWorkbenchLicenseNotifier(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_checked(false)
{
    connect(qnLicensePool, &QnLicensePool::licensesChanged, this,
        &QnWorkbenchLicenseNotifier::checkLicenses);
    connect(accessController(), &QnWorkbenchAccessController::globalPermissionsChanged, this,
        &QnWorkbenchLicenseNotifier::checkLicenses);
    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnWorkbenchLicenseNotifier::at_context_userChanged);
}

QnWorkbenchLicenseNotifier::~QnWorkbenchLicenseNotifier()
{
}

void QnWorkbenchLicenseNotifier::checkLicenses()
{
     if (m_checked)
         return;

    if (!accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
        return;

    QnLicenseWarningStateHash licenseWarningStates = qnSettings->licenseWarningStates();

    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();

    QList<QnLicensePtr> licenses;
    bool warn = false;

    bool someLicenseWillBeBlocked = false;
    for (const auto& runtimeInfo: QnRuntimeInfoManager::instance()->items()->getItems())
    {
        if (runtimeInfo.data.prematureLicenseExperationDate)
        {
            someLicenseWillBeBlocked = true;
            break;
        }
    }

    for (const auto& license: qnLicensePool->getLicenses())
    {
        QnLicense::ErrorCode errorCode;

        if (someLicenseWillBeBlocked
            && !qnLicensePool->isLicenseValid(license, &errorCode)
            && errorCode != QnLicense::Expired)
        {
            licenses.push_back(license);
            licenseWarningStates[license->key()].lastWarningTime = currentTime;
            warn = true;
            continue;
        }

        qint64 expirationTime = license->expirationTime();
        // skip infinite license
        if (expirationTime < 0)
            continue;

        QnLicenseWarningState licenseWarningState = licenseWarningStates.value(license->key());
        // skip already notified expired license
        qint64 lastTimeLeft = expirationTime - licenseWarningState.lastWarningTime;
        if (lastTimeLeft < 0)
            continue;
        // skip license that didn't pass the maximum pre-notification interval
        qint64 timeLeft = expirationTime - currentTime;
        if (timeLeft > kWarningTimes[0])
            continue;

        licenses.push_back(license);

        int lastWarningIndex = 0;
        for (; kWarningTimes[lastWarningIndex] > lastTimeLeft; lastWarningIndex++);

        int warningIndex = 0;
        for (; kWarningTimes[warningIndex] > timeLeft
            && kWarningTimes[warningIndex] > 0; warningIndex++);

        // warn user only if pre-notification interval has been changed.
        if (warningIndex == lastWarningIndex && timeLeft > 0)
            continue;

        warn = true;
        licenseWarningStates[license->key()].lastWarningTime = currentTime;
    }

    if (warn)
    {
        QScopedPointer<QnLicenseNotificationDialog> dialog(new QnLicenseNotificationDialog(mainWindow()));
        dialog->setLicenses(licenses);
        dialog->exec();

        qnSettings->setLicenseWarningStates(licenseWarningStates);
    }

    m_checked = true;
}

void QnWorkbenchLicenseNotifier::at_context_userChanged()
{
    /* Clear flag if we have logged out. */
    if (!context()->user())
        m_checked = false;
}
