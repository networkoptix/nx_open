#include "workbench_update_watcher.h"

#include <QtCore/QTimer>
#include <QtWidgets/QAction>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtGui/QDesktopServices>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <client/client_settings.h>
#include <common/common_module.h>
#include <update/update_checker.h>
#include <ui/dialogs/checkable_message_box.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/common/geometry.h>

#include <utils/common/app_info.h>
#include <utils/common/string.h>

namespace {
    const int updatePeriodMSec = 60 * 60 * 1000; /* 1 hour. */
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

void QnWorkbenchUpdateWatcher::at_checker_updateAvailable(const QnSoftwareVersion &updateVersion, const QUrl &releaseNotesUrl) {
    if (updateVersion.isNull())
        return;

    if (!m_timer->isActive())
        return;

    if (m_latestVersion == updateVersion || qnSettings->ignoredUpdateVersion() >= updateVersion || qnCommon->engineVersion() >= updateVersion)
        return;

    m_latestVersion = updateVersion;

    if (!qnSettings->isAutoCheckForUpdates())
        return;

    QnSoftwareVersion current = qnCommon->engineVersion();
    bool majorVersionChange = updateVersion.major() > current.major() || updateVersion.minor() > current.minor();

    QString title;
    QString message;
    QString actionMessage = tr("Would you like to update?");
    QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Yes | QDialogButtonBox::No;

    if (majorVersionChange) {
        title = tr("Newer version is available");
        message = tr("New version %1 is available.").arg(htmlBold(updateVersion.toString()));
        message += lit("<br/>");
    } else {
        title = tr("Update is recommended");
        message = tr("New version %1 is available.").arg(htmlBold(updateVersion.toString()));
        message += lit("<br/>");
        message += tr("Major issues have been fixed.");
        message += lit("<br/><span style=\"color:%1;\">").arg(qnGlobals->errorTextColor().name());
        message += tr("Update is strongly recommended.");
        message += lit("</span><br/>");
    }

    QnCheckableMessageBox messageBox(0);

    messageBox.setStandardButtons(buttons);
    messageBox.setIconPixmap(QMessageBox::standardIcon(QMessageBox::Question));

#ifdef Q_OS_MAC
    bool hasOutdatedServer = false;
    for (const QnMediaServerResourcePtr &server: qnResPool->getAllServers()) {
        if (server->getVersion() < updateVersion) {
            hasOutdatedServer = true;
            break;
        }
    }
    if (!hasOutdatedServer) {
        actionMessage = tr("Please update %1 Client.").arg(QnAppInfo::productNameLong());
        messageBox.setStandardButtons(QDialogButtonBox::Ok);
        messageBox.setIconPixmap(QMessageBox::standardIcon(QMessageBox::Information));
    }
#endif

    message += actionMessage;

    messageBox.setWindowTitle(title);
    messageBox.setRichText(message);
    messageBox.setCheckBoxText(tr("Don't notify again about this update."));
    setHelpTopic(&messageBox, Qn::Upgrade_Help);

    if (releaseNotesUrl.isValid()) {
        QPushButton *button = messageBox.addButton(tr("Release Notes"), QDialogButtonBox::ActionRole);
        connect(button, &QPushButton::clicked, this, [&releaseNotesUrl]() {
            QDesktopServices::openUrl(releaseNotesUrl);
        });
    }

    //TODO: #dklychkov refactor update notifications in 2.4
    messageBox.adjustSize();
    messageBox.setGeometry(QnGeometry::aligned(messageBox.size(), mainWindow()->geometry(), Qt::AlignCenter));

    messageBox.exec();

    /* We check for 'Yes' button. 'No' and even 'Ok' buttons are considered negative. */
    if (messageBox.clickedStandardButton() == QDialogButtonBox::Yes) {
        action(Qn::SystemUpdateAction)->trigger();
    } else {
        qnSettings->setIgnoredUpdateVersion(messageBox.isChecked() ? updateVersion : QnSoftwareVersion());
    }
}
