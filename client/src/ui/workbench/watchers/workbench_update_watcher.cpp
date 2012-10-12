#include "workbench_update_watcher.h"

#include <QtCore/QTimer>

#include <utils/appcast/update_checker.h>

#include "version.h"

namespace {
    const int updatePeriodMSec = 60 * 60 * 1000; /* 1 hour. */
}

QnWorkbenchUpdateWatcher::QnWorkbenchUpdateWatcher(QObject *parent):
    QObject(parent)
{
    m_checker = new QnUpdateChecker(
        QUrl(QLatin1String("http://networkoptix.com/files/hdwitnesscast.xml")), 
        QString(QLatin1String("%1-%2")).arg(QLatin1String(APPLICATION_PLATFORM)).arg(QLatin1String(APPLICATION_ARCH)), 
        QLatin1String(APPLICATION_VERSION), 
        this
    );
    connect(m_checker, SIGNAL(updatesAvailable(QnUpdateInfoItemList)), this, SLOT(at_checker_updatesAvailable(QnUpdateInfoItemList)));

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(at_timer_timeout()));
    timer->start(updatePeriodMSec);

    at_timer_timeout();
}

QnWorkbenchUpdateWatcher::~QnWorkbenchUpdateWatcher() {
    return;
}

void QnWorkbenchUpdateWatcher::at_checker_updatesAvailable(QnUpdateInfoItemList updates) {
    if(m_availableUpdates == updates)
        return;
}

void QnWorkbenchUpdateWatcher::at_timer_timeout() {

}
