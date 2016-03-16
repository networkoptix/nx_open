#include "workbench_update_watcher.h"

#include <QtCore/QTimer>
#include <QtWidgets/QAction>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtGui/QDesktopServices>

#include <api/global_settings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <client/client_settings.h>
#include <common/common_module.h>

#include <ui/actions/action_manager.h>
#include <ui/dialogs/message_box.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/common/geometry.h>

#include <update/update_checker.h>
#include <update/update_info.h>

#include <utils/common/app_info.h>
#include <utils/common/string.h>
#include <utils/common/util.h>

namespace {
    const int kUpdatePeriodMSec = 60 * 60 * 1000; /* 1 hour. */

    const int kHoursPerDay = 24;
    const int kMinutesPerHour = 60;
    const int kSecsPerMinute = 60;

    const int kTooLateDayOfWeek = Qt::Thursday;

} // anonymous namespace

QnWorkbenchUpdateWatcher::QnWorkbenchUpdateWatcher(QObject *parent)
    : QObject(parent)
    , QnWorkbenchContextAware(parent)
    , m_checker(nullptr)
    , m_timer(new QTimer(this))
    , m_notifiedVersion()
{
    QUrl updateFeedUrl(qnSettings->updateFeedUrl());
    if(updateFeedUrl.isEmpty())
        return;

    m_checker = new QnUpdateChecker(updateFeedUrl, this);
    connect(m_checker, &QnUpdateChecker::updateAvailable, this, &QnWorkbenchUpdateWatcher::at_checker_updateAvailable);

    m_timer->setInterval(kUpdatePeriodMSec);
    connect(m_timer, &QTimer::timeout, m_checker, &QnUpdateChecker::checkForUpdates);
}

QnWorkbenchUpdateWatcher::~QnWorkbenchUpdateWatcher() {}

void QnWorkbenchUpdateWatcher::start()
{
    if (!m_checker)
        return;

    m_timer->start();
    m_checker->checkForUpdates();
}

void QnWorkbenchUpdateWatcher::stop()
{
    m_timer->stop();
}

void QnWorkbenchUpdateWatcher::at_checker_updateAvailable(const QnUpdateInfo &info)
{
    NX_ASSERT(!info.currentRelease.isNull(), Q_FUNC_INFO, "Notification must be valid");

    if (info.currentRelease.isNull())
        return;

    /* We are not interested in updates right now. */
    if (!m_timer->isActive())
        return;

    /* We have no access rights. */
    if (!menu()->canTrigger(QnActions::SystemUpdateAction))
        return;

    /* User was already notified about this release. */
    if (m_notifiedVersion == info.currentRelease)
        return;

    /* Current version is greater or equal to latest. */
    if (qnCommon->engineVersion() >= info.currentRelease)
        return;

    /* User is not interested in this update. */
    if (qnSettings->ignoredUpdateVersion() >= info.currentRelease)
        return;

    /* Administrator disabled update notifications globally. */
    if (!QnGlobalSettings::instance()->isUpdateNotificationsEnabled())
        return;

    /* Do not show notifications near the end of the week or on our holidays. */
     if (QDateTime::currentDateTime().date().dayOfWeek() >= kTooLateDayOfWeek)
         return;

    QnUpdateInfo oldUpdateInfo = qnSettings->latestUpdateInfo();
    if (oldUpdateInfo.currentRelease != info.currentRelease
        ||
        oldUpdateInfo.releaseDateMs != info.releaseDateMs
        ||
        oldUpdateInfo.releaseDeliveryDays != info.releaseDeliveryDays)
    {
        /* New release was published - or we decided to change delivery period. Estimating new delivery date. */
        QDateTime releaseDate = QDateTime::fromMSecsSinceEpoch(info.releaseDateMs);

        /* We do not need high precision, selecting time to deliver. */
        int timeToDeliverMinutes = random(0, info.releaseDeliveryDays * kHoursPerDay * kMinutesPerHour);
        qint64 timeToDeliverMs = timeToDeliverMinutes * kSecsPerMinute;
        qnSettings->setUpdateDeliveryDate(releaseDate.addSecs(timeToDeliverMs).toMSecsSinceEpoch());
    }
    qnSettings->setLatestUpdateInfo(info);

    /* Update is postponed */
    if (qnSettings->updateDeliveryDate() > QDateTime::currentMSecsSinceEpoch())
        return;

    showUpdateNotification(info);
}

void QnWorkbenchUpdateWatcher::showUpdateNotification(const QnUpdateInfo &info)
{
    m_notifiedVersion = info.currentRelease;

    QnSoftwareVersion current = qnCommon->engineVersion();
    bool majorVersionChange = info.currentRelease.major() > current.major() || info.currentRelease.minor() > current.minor();

    QString title;
    QString message;
    QString actionMessage = tr("Would you like to update?");
    QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Yes | QDialogButtonBox::No;

    if (majorVersionChange)
    {
        title = tr("A newer version is available.");
        message = tr("New version %1 is available.").arg(htmlBold(info.currentRelease.toString()));
        message += lit("<br/>");
    }
    else
    {
        title = tr("Update is recommended.");
        message = tr("New version %1 is available.").arg(htmlBold(info.currentRelease.toString()));
        message += lit("<br/>");
        message += tr("Major issues have been fixed.");
        message += lit("<br/><span style=\"color:%1;\">").arg(qnGlobals->errorTextColor().name());
        message += tr("Update is strongly recommended.");
        message += lit("</span><br/>");
    }

    QnMessageBox messageBox(mainWindow());

    messageBox.setStandardButtons(buttons);
    messageBox.setIcon(QnMessageBox::Question);

#ifdef Q_OS_MAC
    bool hasOutdatedServer = false;
    for (const QnMediaServerResourcePtr &server: qnResPool->getAllServers(Qn::AnyStatus))
    {
        if (server->getVersion() < info.currentRelease)
        {
            hasOutdatedServer = true;
            break;
        }
    }
    if (!hasOutdatedServer)
    {
        actionMessage = tr("Please update %1 Client.").arg(QnAppInfo::productNameLong());
        messageBox.setStandardButtons(QDialogButtonBox::Ok);
        messageBox.setIcon(QMessageBox::Information);
    }
#endif

    message += actionMessage;

    messageBox.setWindowTitle(title);
    messageBox.setTextFormat(Qt::RichText);
    messageBox.setText(message);
    messageBox.setCheckBoxText(tr("Do not notify me again about this update."));
    setHelpTopic(&messageBox, Qn::Upgrade_Help);

    if (info.releaseNotesUrl.isValid())
    {
        QPushButton* button = messageBox.addButton(tr("Release Notes"), QDialogButtonBox::ActionRole);
        connect(button, &QPushButton::clicked, this, [info]
        {
            QDesktopServices::openUrl(info.releaseNotesUrl);
        });
    }

    //TODO: #dklychkov refactor update notifications in 2.4
    messageBox.adjustSize();
    messageBox.setGeometry(QnGeometry::aligned(messageBox.size(), mainWindow()->geometry(), Qt::AlignCenter));

    int result = messageBox.exec();

    /* We check for 'Yes' button. 'No' and even 'Ok' buttons are considered negative. */
    if (result == QDialogButtonBox::Yes)
        action(QnActions::SystemUpdateAction)->trigger();
    else
        qnSettings->setIgnoredUpdateVersion(messageBox.isChecked() ? info.currentRelease : QnSoftwareVersion());
}
