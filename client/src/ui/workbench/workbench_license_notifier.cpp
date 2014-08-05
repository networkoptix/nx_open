#include "workbench_license_notifier.h"

#include <core/resource/user_resource.h>
#include <licensing/license.h>
#include <client/client_settings.h>
#include <utils/common/synctime.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/dialogs/license_notification_dialog.h>
#include "api/app_server_connection.h"
#include "common/common_module.h"
#include "api/runtime_info_manager.h"

namespace {
    const qint64 day = 1000ll * 60ll * 60ll * 24ll;

    const qint64 warningTimes[] = {
        day * 15,
        day * 10,
        day * 5,
        day * 3,
        day * 2,
        day,
        0
    };

} // anonymous namespace


QnWorkbenchLicenseNotifier::QnWorkbenchLicenseNotifier(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_checked(false)
{
    connect(qnLicensePool, SIGNAL(licensesChanged()), this, SLOT(at_licensePool_licensesChanged()));
    connect(context(), SIGNAL(userChanged(const QnUserResourcePtr &)), this, SLOT(at_context_userChanged()));
}

QnWorkbenchLicenseNotifier::~QnWorkbenchLicenseNotifier() {
    return;
}

void QnWorkbenchLicenseNotifier::checkLicenses() {
    if (!context()->accessController()->hasGlobalPermissions(Qn::GlobalProtectedPermission))
        return;

    QnLicenseWarningStateHash licenseWarningStates = qnSettings->licenseWarningStates();

    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();

    QList<QnLicensePtr> licenses;
    bool warn = false;

    bool someLicenseWillBeBlocked = false;
    foreach (const QnPeerRuntimeInfo& runtimeInfo, QnRuntimeInfoManager::instance()->items()->getItems())
    {
        if (runtimeInfo.data.prematureLicenseExperationDate)
            someLicenseWillBeBlocked = true;
    }

    foreach(const QnLicensePtr &license, qnLicensePool->getLicenses()) 
    {
        if (someLicenseWillBeBlocked && !qnLicensePool->isLicenseValid(license))
        {
            licenses.push_back(license);
            warn = true;
            continue;
        }

        qint64 expirationTime = license->expirationTime();
        // skip infinite license
        if(expirationTime < 0)
            continue;

        QnLicenseWarningState licenseWarningState = licenseWarningStates.value(license->key());
        // skip already notified expired license
        qint64 lastTimeLeft = expirationTime - licenseWarningState.lastWarningTime;
        if(lastTimeLeft < 0)
            continue;
        // skip license that didn't pass the maximum pre-notification interval
        qint64 timeLeft = expirationTime - currentTime;
        if (timeLeft > warningTimes[0])
            continue;

        licenses.push_back(license);

        int lastWarningIndex = 0;
        for(; warningTimes[lastWarningIndex] > lastTimeLeft; lastWarningIndex++);

        int warningIndex = 0;
        for(; warningTimes[warningIndex] > timeLeft && warningTimes[warningIndex] > 0; warningIndex++);

        // warn user only if pre-notification interval has been changed.
        if (warningIndex == lastWarningIndex && timeLeft > 0)
            continue;

        warn = true;
        licenseWarningStates[license->key()].lastWarningTime = currentTime;
    }

    if(warn) {
        QScopedPointer<QnLicenseNotificationDialog> dialog(new QnLicenseNotificationDialog(mainWindow()));
        dialog->setLicenses(licenses);
        dialog->exec();

        qnSettings->setLicenseWarningStates(licenseWarningStates);
    }
}

void QnWorkbenchLicenseNotifier::at_licensePool_licensesChanged() {
    if(!m_checked) {
        m_checked = true;
        checkLicenses();
    }
}

void QnWorkbenchLicenseNotifier::at_context_userChanged() {
    if(context()->user())
        m_checked = false;
    if(!qnLicensePool->getLicenses().isEmpty() && !m_checked) {
        m_checked = true;
        checkLicenses();
    }
}
