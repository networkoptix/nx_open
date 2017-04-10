#include "workbench_license_notifier.h"

#include <api/runtime_info_manager.h>

#include <client/client_settings.h>

#include <core/resource/user_resource.h>

#include <licensing/license.h>
#include <licensing/license_validator.h>

#include <ui/dialogs/license_notification_dialog.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/synctime.h>
#include <utils/common/delayed.h>

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
    QnWorkbenchContextAware(parent),
    m_checked(false)
{
    auto checkLicensesDelayed =
        [this, guard = QPointer<QnWorkbenchLicenseNotifier>(this)]()
        {
            if (!guard)
                return;

            executeDelayedParented([this]{ checkLicenses(); }, kMessagesDelayMs, this);
        };

    connect(licensePool(), &QnLicensePool::licensesChanged, this, checkLicensesDelayed);
    connect(accessController(), &QnWorkbenchAccessController::globalPermissionsChanged, this,
        checkLicensesDelayed);
    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnWorkbenchLicenseNotifier::at_context_userChanged);
}

QnWorkbenchLicenseNotifier::~QnWorkbenchLicenseNotifier()
{
}

void QnWorkbenchLicenseNotifier::checkLicenses()
{
    if (context()->closingDown())
        return;

    if (!accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
        return;

    if (m_checked)
        return;

     m_checked = true;

    QnLicenseWarningStateHash licenseWarningStates = qnSettings->licenseWarningStates();

    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();

    QList<QnLicensePtr> licenses;
    bool warn = false;

    bool someLicenseWillBeBlocked = false;
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
        QnLicenseErrorCode errorCode = licensePool()->validateLicense(license);

        if (someLicenseWillBeBlocked
            && errorCode != QnLicenseErrorCode::NoError
            && errorCode != QnLicenseErrorCode::Expired)
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

    if (warn && mainWindow())
    {
        QScopedPointer<QnLicenseNotificationDialog> dialog(new QnLicenseNotificationDialog(mainWindow()));
        dialog->setLicenses(licenses);
        dialog->exec();

        qnSettings->setLicenseWarningStates(licenseWarningStates);
    }
}

void QnWorkbenchLicenseNotifier::at_context_userChanged()
{
    /* Clear flag if we have logged out. */
    if (!context()->user())
        m_checked = false;
}
