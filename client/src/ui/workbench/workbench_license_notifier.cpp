#include "workbench_license_notifier.h"

#include <licensing/license.h>
#include <utils/settings.h>
#include <utils/common/synctime.h>

#include <ui/dialogs/license_notification_dialog.h>

namespace {
    const qint64 day = 1000ll * 60ll * 60ll * 24ll;

    const qint64 warningTimes[] = {
        day * 30,
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
    QnWorkbenchContextAware(parent)
{
    connect(qnLicensePool, SIGNAL(licensesChanged()), this, SLOT(at_licensePool_licensesChanged()));
}

QnWorkbenchLicenseNotifier::~QnWorkbenchLicenseNotifier() {
    return;
}

void QnWorkbenchLicenseNotifier::at_licensePool_licensesChanged() {
    QnLicenseWarningStateHash licenseWarningStates = qnSettings->licenseWarningStates();

    QDateTime now = qnSyncTime->currentDateTime();

    QList<QnLicensePtr> licenses;
    bool warn = false;

    foreach(const QnLicensePtr &license, qnLicensePool->getLicenses().licenses()) {
        QDateTime expiration = license->expirationDate();
        if(expiration.isNull())
            continue;

        licenses.push_back(license);

        QnLicenseWarningState licenseWarningState = licenseWarningStates.value(license->key());
        if(licenseWarningState.ignore)
            continue;

        int nextWarningIndex = 0;
        if(!licenseWarningState.lastWarningTime.isNull()) {
            qint64 lastTimeLeft = licenseWarningState.lastWarningTime.msecsTo(expiration);
            if(lastTimeLeft < 0)
                continue;
            for(; warningTimes[nextWarningIndex] > lastTimeLeft; nextWarningIndex++);
        }

        qint64 timeLeft = now.msecsTo(expiration);
        if(warningTimes[nextWarningIndex] == 0 || timeLeft < warningTimes[nextWarningIndex]) {
            licenseWarningStates[license->key()].lastWarningTime = now;
            warn = true;
        }
    }

    if(warn) {
        QScopedPointer<QnLicenseNotificationDialog> dialog(new QnLicenseNotificationDialog());
        dialog->setLicenses(licenses);
        dialog->exec();

        qnSettings->setLicenseWarningStates(licenseWarningStates);
    }
}

