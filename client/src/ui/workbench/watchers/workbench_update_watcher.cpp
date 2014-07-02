#include "workbench_update_watcher.h"

#include <QtCore/QTimer>

#include <client/client_settings.h>
#include <common/common_module.h>
#include <utils/update_checker.h>

#include "version.h"

namespace {
    const int updatePeriodMSec = 60 * 60 * 1000; /* 1 hour. */
    const QString QN_UPDATES_URL = lit("http://enk.me/bg/dklychkov/exmaple_update/updates");
} // anonymous namespace

QnWorkbenchUpdateWatcher::QnWorkbenchUpdateWatcher(QObject *parent):
    QObject(parent),
    m_checker(NULL)
{
//    QUrl updateFeedUrl(qnSettings->updateFeedUrl());
    QUrl updateFeedUrl(QN_UPDATES_URL);
    if(updateFeedUrl.isEmpty())
        return; 

    m_checker = new QnUpdateChecker(updateFeedUrl, this);
    connect(m_checker, &QnUpdateChecker::updateAvailable, this, &QnWorkbenchUpdateWatcher::at_checker_updateAvailable);

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(at_timer_timeout()));
    timer->start(updatePeriodMSec);

    at_timer_timeout();
}

QnWorkbenchUpdateWatcher::~QnWorkbenchUpdateWatcher() {}

void QnWorkbenchUpdateWatcher::at_checker_updateAvailable(const QnSoftwareVersion &version) {
    if (version.isNull())
        return;

    if (m_availableUpdate == version)
        return;

    m_availableUpdate = version;
    emit availableUpdateChanged();
}

void QnWorkbenchUpdateWatcher::at_timer_timeout() {
    m_checker->checkForUpdates();
}
