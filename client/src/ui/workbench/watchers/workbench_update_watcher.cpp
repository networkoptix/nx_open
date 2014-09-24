#include "workbench_update_watcher.h"

#include <QtCore/QTimer>

#include <client/client_settings.h>
#include <common/common_module.h>
#include <update/update_checker.h>

namespace {
    const int updatePeriodMSec = 60 * 60 * 1000; /* 1 hour. */
} // anonymous namespace

QnWorkbenchUpdateWatcher::QnWorkbenchUpdateWatcher(QObject *parent):
    QObject(parent),
    m_checker(NULL)
{
    QUrl updateFeedUrl(qnSettings->updateFeedUrl());
    if(updateFeedUrl.isEmpty())
        return; 

    m_checker = new QnUpdateChecker(updateFeedUrl, this);
    connect(m_checker, &QnUpdateChecker::updateAvailable, this, &QnWorkbenchUpdateWatcher::at_checker_updateAvailable);

    m_timer = new QTimer(this);
    m_timer->setInterval(updatePeriodMSec);
    connect(m_timer, &QTimer::timeout, m_checker, &QnUpdateChecker::checkForUpdates);
}

QnWorkbenchUpdateWatcher::~QnWorkbenchUpdateWatcher() {}

void QnWorkbenchUpdateWatcher::start() {
    m_checker->checkForUpdates();
    m_timer->start();
}

void QnWorkbenchUpdateWatcher::stop() {
    m_timer->stop();
}

void QnWorkbenchUpdateWatcher::at_checker_updateAvailable(const QnSoftwareVersion &version) {
    if (version.isNull())
        return;

    if (!m_timer->isActive())
        return;

    if (m_availableUpdate == version)
        return;

    m_availableUpdate = version;
    emit availableUpdateChanged();
}
