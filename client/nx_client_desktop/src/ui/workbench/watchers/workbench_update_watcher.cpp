#include "workbench_update_watcher.h"

#include <QtCore/QTimer>
#include <QtWidgets/QAction>
#include <QtWidgets/QPushButton>
#include <QtGui/QDesktopServices>

#include <api/global_settings.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <client/client_settings.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>

#include <update/update_checker.h>
#include <update/update_info.h>

#include <utils/common/html.h>

#include <utils/common/app_info.h>
#include <utils/common/util.h>
#include <nx/utils/random.h>

using namespace nx::client::desktop::ui;

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
    connect(m_checker, &QnUpdateChecker::updateAvailable, this,
        &QnWorkbenchUpdateWatcher::at_checker_updateAvailable);

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
    if (!menu()->canTrigger(action::SystemUpdateAction))
        return;

    /* User was already notified about this release. */
    if (m_notifiedVersion == info.currentRelease)
        return;

    /* Current version is greater or equal to latest. */
    if (qnStaticCommon->engineVersion() >= info.currentRelease)
        return;

    /* User is not interested in this update. */
    if (qnSettings->ignoredUpdateVersion() >= info.currentRelease)
        return;

    /* Administrator disabled update notifications globally. */
    if (!qnGlobalSettings->isUpdateNotificationsEnabled())
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
        int timeToDeliverMinutes = nx::utils::random::number(0, info.releaseDeliveryDays * kHoursPerDay * kMinutesPerHour);
        qint64 timeToDeliverMs = timeToDeliverMinutes * kSecsPerMinute;
        qnSettings->setUpdateDeliveryDate(releaseDate.addSecs(timeToDeliverMs).toMSecsSinceEpoch());
    }
    qnSettings->setLatestUpdateInfo(info);
    qnSettings->save();

    /* Update is postponed */
    if (qnSettings->updateDeliveryDate() > QDateTime::currentMSecsSinceEpoch())
        return;

    showUpdateNotification(info);
}

void QnWorkbenchUpdateWatcher::showUpdateNotification(const QnUpdateInfo &info)
{
    m_notifiedVersion = info.currentRelease;

    const QnSoftwareVersion current = qnStaticCommon->engineVersion();
    const bool majorVersionChange = ((info.currentRelease.major() > current.major())
        || (info.currentRelease.minor() > current.minor()));

    const auto text = tr("%1 version available").arg(info.currentRelease.toString());
    const auto releaseNotesLink = makeHref(tr("Release Notes"), info.releaseNotesUrl);

    QStringList extras;
    if (majorVersionChange)
        extras.push_back(tr("Major issues have been fixed. Update is strongly recommended."));
    if (!info.description.isEmpty())
        extras.push_back(info.description);
    extras.push_back(releaseNotesLink);

    QnMessageBox messageBox(QnMessageBoxIcon::Information,
        text,
        makeHtml(extras.join(lit("<br>"))),
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        mainWindowWidget());

    messageBox.addButton(tr("Update..."), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
    messageBox.setCustomCheckBoxText(tr("Do not notify again about this update"));

    const auto result = messageBox.exec();

    if (messageBox.isChecked())
    {
        qnSettings->setIgnoredUpdateVersion(info.currentRelease);
        qnSettings->save();
    }

    if (result != QDialogButtonBox::Cancel)
        action(action::SystemUpdateAction)->trigger();
}
