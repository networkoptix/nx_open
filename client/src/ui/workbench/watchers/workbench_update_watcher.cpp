#include "workbench_update_watcher.h"

#include <QtCore/QTimer>
#include <QtWidgets/QAction>
#include <QtWidgets/QMessageBox>

#include <client/client_settings.h>
#include <common/common_module.h>
#include <update/update_checker.h>
#include <ui/dialogs/checkable_message_box.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

namespace {
    const int updatePeriodMSec = 60 * 60 * 1000; /* 1 hour. */
    QColor redTextColor = Qt::red; // TODO: #dklychkov make it customizable
} // anonymous namespace

QnWorkbenchUpdateWatcher::QnWorkbenchUpdateWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
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

void QnWorkbenchUpdateWatcher::at_checker_updateAvailable(const QnSoftwareVersion &updateVersion) {
    if (updateVersion.isNull())
        return;

    if (!m_timer->isActive())
        return;

    if (m_latestVersion == updateVersion || qnSettings->ignoredUpdateVersion() >= updateVersion)
        return;

    m_latestVersion = updateVersion;

    if (!qnSettings->isAutoCheckForUpdates())
        return;

    QnSoftwareVersion current = qnCommon->engineVersion();
    bool majorVersionChange = updateVersion.major() > current.major() || updateVersion.minor() > current.minor();

    QString title;
    QString message;
    if (majorVersionChange) {
        title = tr("Newer version is available");
        message = tr("New version <b>%1</b> is available.").arg(updateVersion.toString());
        message += lit("<br/>");
        message += tr("Would you like to update?");
    } else {
        title = tr("Update is recommended");
        message = tr("New version <b>%1</b> is available.").arg(updateVersion.toString());
        message += lit("<br/>");
        message += tr("Major issues have been fixed.");
        message += lit("<br/><span style=\"color:%1;\">").arg(redTextColor.name());
        message += tr("Update is strongly recommended.");
        message += lit("</span><br/>");
        message += tr("Would you like to update?");
    }

    QnCheckableMessageBox messageBox(mainWindow());
    messageBox.setWindowTitle(title);
    messageBox.setIconPixmap(QMessageBox::standardIcon(QMessageBox::Question));
    messageBox.setRichText(message);
    messageBox.setCheckBoxText(tr("Don't notify again about this update."));
    messageBox.setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No);
    setHelpTopic(&messageBox, Qn::Upgrade_Help);
    int res = messageBox.exec();

    if (res == QnCheckableMessageBox::Accepted) {
        action(Qn::SystemUpdateAction)->trigger();
    } else {
        qnSettings->setIgnoredUpdateVersion(messageBox.isChecked() ? updateVersion : QnSoftwareVersion());
    }
}
