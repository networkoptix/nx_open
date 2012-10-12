#include "workbench_update_watcher.h"

#include <utils/appcast/update_checker.h>

#include "version.h"

QnWorkbenchUpdateWatcher::QnWorkbenchUpdateWatcher(QObject *parent):
    QObject(parent)
{
    m_checker = new QnUpdateChecker(
        QUrl("http://networkoptix.com/files/hdwitnesscast.xml"), 
        QString(QLatin1String("%1-%2")).arg(QLatin1String(APPLICATION_PLATFORM)).arg(QLatin1String(APPLICATION_ARCH)), 
        QLatin1String(APPLICATION_VERSION), 
        this
    );
}

QnWorkbenchUpdateWatcher::~QnWorkbenchUpdateWatcher() {

}

void QnWorkbenchUpdateWatcher::at_checker_updatesAvailable() {

}

void QnWorkbenchUpdateWatcher::at_timer_timeout() {

}
