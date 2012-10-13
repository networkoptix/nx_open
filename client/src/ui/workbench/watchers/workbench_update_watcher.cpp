#include "workbench_update_watcher.h"

#include <QtCore/QTimer>

#include <utils/appcast/update_checker.h>

#include "version.h"

namespace {
    struct UpdateVersionLess {
        bool operator()(const QnUpdateInfoItem &l, const QnUpdateInfoItem &r) {
            return l.version < r.version;
        }
    };

    const int updatePeriodMSec = 60 * 60 * 1000; /* 1 hour. */
} // anonymous namespace

QnWorkbenchUpdateWatcher::QnWorkbenchUpdateWatcher(QObject *parent):
    QObject(parent)
{
    m_checker = new QnUpdateChecker(
        QUrl(QLatin1String("http://networkoptix.com/files/hdwitnesscast.xml")), 
        QString(QLatin1String("%1-%2")).arg(QLatin1String(QN_APPLICATION_PLATFORM)).arg(QLatin1String(QN_APPLICATION_ARCH)), 
        QLatin1String(QN_ENGINE_VERSION), 
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
    if(updates.isEmpty())
        return;

    qSort(updates.begin(), updates.end(), UpdateVersionLess());
    QnUpdateInfoItem lastUpdate = updates.last();

    QnVersion currentVersion = QnVersion(QLatin1String(QN_ENGINE_VERSION));
    if(lastUpdate.version == currentVersion || lastUpdate.version < currentVersion) // TODO: use <=
        return;

    if(lastUpdate.version == m_availableUpdate.version)
        return;

    m_availableUpdate = lastUpdate;
    emit availableUpdateChanged();
}

void QnWorkbenchUpdateWatcher::at_timer_timeout() {
    m_checker->checkForUpdates();
}
