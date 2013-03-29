#include "workbench_license_notifier.h"

#include <core/resource/user_resource.h>
#include <licensing/license.h>
#include <utils/settings.h>
#include <utils/common/synctime.h>

#include <ui/workbench/workbench_context.h>
#include <ui/dialogs/license_notification_dialog.h>

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
    QnLicenseWarningStateHash licenseWarningStates = qnSettings->licenseWarningStates();

    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();

    QList<QnLicensePtr> licenses;
    bool warn = false;

    foreach(const QnLicensePtr &license, qnLicensePool->getLicenses().licenses()) {
        qint64 expirationTime = license->expirationTime();
        if(expirationTime < 0)
            continue;

        licenses.push_back(license);

        QnLicenseWarningState licenseWarningState = licenseWarningStates.value(license->key());
        if(licenseWarningState.ignore)
            continue;

        qint64 lastTimeLeft = expirationTime - licenseWarningState.lastWarningTime;
        if(lastTimeLeft < 0)
            continue;
        int nextWarningIndex = 0;
        for(; warningTimes[nextWarningIndex] > lastTimeLeft; nextWarningIndex++);

        qint64 timeLeft = expirationTime - currentTime;
        if(warningTimes[nextWarningIndex] == 0 || timeLeft < warningTimes[nextWarningIndex]) {
            warn = true;
            licenseWarningStates[license->key()].lastWarningTime = currentTime;
        }
    }

    if(warn) {
        QScopedPointer<QnLicenseNotificationDialog> dialog(new QnLicenseNotificationDialog());
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
