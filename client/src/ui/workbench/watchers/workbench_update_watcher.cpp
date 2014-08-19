#include "workbench_update_watcher.h"

#include <QtCore/QTimer>

#include <utils/appcast/update_checker.h>
#include <utils/common/warnings.h>
#include <client/client_settings.h>
#include <common/common_module.h>

#include "version.h"

namespace {
    struct UpdateEngineVersionLess {
        bool operator()(const QnUpdateInfoItem &l, const QnUpdateInfoItem &r) {
            return l.engineVersion < r.engineVersion;
        }
    };

    const int updatePeriodMSec = 60 * 60 * 1000; /* 1 hour. */
} // anonymous namespace

QnWorkbenchUpdateWatcher::QnWorkbenchUpdateWatcher(QObject *parent):
    QObject(parent),
    m_checker(NULL)
{
    QUrl updateFeedUrl(qnSettings->updateFeedUrl());
    if(updateFeedUrl.isEmpty())
        return; 

    QString platform = QString(QLatin1String("%1-%2")).arg(QLatin1String(QN_APPLICATION_PLATFORM)).arg(QLatin1String(QN_APPLICATION_ARCH));
    QString version = qnCommon->engineVersion().toString();

    qnDebug("Settings up update watcher for %1 build of version %2 at %3.", platform, version, updateFeedUrl);

    m_checker = new QnUpdateChecker(updateFeedUrl, platform, version, this);
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

    qSort(updates.begin(), updates.end(), UpdateEngineVersionLess());
    QnUpdateInfoItem lastUpdate = updates.last();

    QnSoftwareVersion currentVersion = qnCommon->engineVersion();
    if(lastUpdate.engineVersion <= currentVersion)
        return;

    if(lastUpdate.engineVersion == m_availableUpdate.engineVersion)
        return;

    m_availableUpdate = lastUpdate;
    emit availableUpdateChanged();
}

void QnWorkbenchUpdateWatcher::at_timer_timeout() {
    m_checker->checkForUpdates();
}
