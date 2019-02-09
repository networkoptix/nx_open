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

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/system_update/update_contents.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>

#include <utils/common/html.h>

#include <utils/common/app_info.h>
#include <utils/common/util.h>
#include <nx/utils/random.h>
#include <nx/utils/guarded_callback.h>

#include <nx/update/update_information.h>
#include <nx/update/update_check.h>

using namespace nx::vms::client::desktop::ui;
using UpdateContents = nx::update::UpdateContents;

namespace {
    constexpr int kUpdatePeriodMSec = 60 * 60 * 1000; //< 1 hour.
    constexpr int kHoursPerDay = 24;
    constexpr int kMinutesPerHour = 60;
    constexpr int kSecsPerMinute = 60;
    constexpr int kTooLateDayOfWeek = Qt::Thursday;
} // anonymous namespace

struct QnWorkbenchUpdateWatcher::Private
{
    UpdateContents updateContents;
    std::future<UpdateContents> m_updateCheck;
};

QnWorkbenchUpdateWatcher::QnWorkbenchUpdateWatcher(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_timer(new QTimer(this)),
    m_notifiedVersion(),
    m_private(new Private())
{
    m_timer->setInterval(kUpdatePeriodMSec);

    m_autoChecksEnabled = qnGlobalSettings->isUpdateNotificationsEnabled();
    connect(m_timer, &QTimer::timeout, this, &QnWorkbenchUpdateWatcher::atStartCheckUpdate);
    connect(qnGlobalSettings, &QnGlobalSettings::updateNotificationsChanged, this,
        [this]()
        {
            m_autoChecksEnabled = qnGlobalSettings->isUpdateNotificationsEnabled();
            syncState();
        });
}

QnWorkbenchUpdateWatcher::~QnWorkbenchUpdateWatcher() {}

void QnWorkbenchUpdateWatcher::syncState()
{
    if (m_userLoggedIn && m_autoChecksEnabled && !m_timer->isActive())
    {
        NX_VERBOSE(this, "syncState() - starting automatic checks for updates");
        m_timer->start();
        atStartCheckUpdate();
    }

    if ((!m_userLoggedIn || !m_autoChecksEnabled) && m_timer->isActive())
    {
        NX_VERBOSE(this, "syncState() - stopping automatic checks for updates");
        m_timer->stop();
    }
}

void QnWorkbenchUpdateWatcher::start()
{
    m_userLoggedIn = true;
    syncState();
}

void QnWorkbenchUpdateWatcher::stop()
{
    m_userLoggedIn = false;
    syncState();
}

void QnWorkbenchUpdateWatcher::atStartCheckUpdate()
{
    // This signal will be removed when update check is complete.
    if (m_private->m_updateCheck.valid())
        return;
    QString updateUrl = qnSettings->updateFeedUrl();
    NX_ASSERT(!updateUrl.isEmpty());

    auto callback = nx::utils::guarded(this,
        [this](const UpdateContents& contents)
        {
            atCheckerUpdateAvailable(contents);
        });

    m_private->m_updateCheck = nx::update::checkLatestUpdate(
        updateUrl,
        commonModule()->engineVersion(),
        std::move(callback));
}

void QnWorkbenchUpdateWatcher::atCheckerUpdateAvailable(const UpdateContents& contents)
{
    if (!qnGlobalSettings->isUpdateNotificationsEnabled())
        return;

    m_private->m_updateCheck = std::future<UpdateContents>();
    m_private->updateContents = contents;

    // 'NoNewVersion' is considered invalid as well.
    if (!contents.info.isValid())
        return;

    // We are not interested in updates right now.
    if (!m_timer->isActive())
        return;

    // We have no access rights.
    if (!menu()->canTrigger(action::SystemUpdateAction))
        return;

    auto targetVersion = contents.getVersion();
    // User was already notified about this release.
    if (m_notifiedVersion == targetVersion)
        return;

    // Current version is greater or equal to latest.
    if (commonModule()->engineVersion() >= targetVersion)
        return;

    // User is not interested in this update.
    if (qnSettings->ignoredUpdateVersion() >= targetVersion)
        return;

    // Administrator disabled update notifications globally.
    if (!qnGlobalSettings->isUpdateNotificationsEnabled())
        return;

    // Do not show notifications near the end of the week or on our holidays.
     if (QDateTime::currentDateTime().date().dayOfWeek() >= kTooLateDayOfWeek)
         return;

    // TODO: Should inject this values inside UpdateInformation
    // Release date - in msecs since epoch.
    qint64 releaseDateMs = contents.info.releaseDateMs;
    // Maximum days for release delivery.
    int releaseDeliveryDays = contents.info.releaseDeliveryDays;

    nx::update::Information oldUpdateInfo = qnSettings->latestUpdateInfo();
    if (nx::utils::SoftwareVersion(oldUpdateInfo.version) != targetVersion
        || oldUpdateInfo.releaseDateMs != releaseDateMs
        || oldUpdateInfo.releaseDeliveryDays != releaseDeliveryDays)
    {
        // New release was published - or we decided to change delivery period.
        // Estimating new delivery date.
        QDateTime releaseDate = QDateTime::fromMSecsSinceEpoch(releaseDateMs);

        // We do not need high precision, selecting time to deliver.
        int timeToDeliverMinutes = nx::utils::random::number(0,
            releaseDeliveryDays * kHoursPerDay * kMinutesPerHour);
        qint64 timeToDeliverMs = timeToDeliverMinutes * kSecsPerMinute;
        qnSettings->setUpdateDeliveryDate(releaseDate.addSecs(timeToDeliverMs).toMSecsSinceEpoch());
    }
    qnSettings->setLatestUpdateInfo(contents.info);
    qnSettings->save();

    // Update is postponed
    if (qnSettings->updateDeliveryDate() > QDateTime::currentMSecsSinceEpoch())
        return;

    showUpdateNotification(targetVersion, contents.info.releaseNotesUrl, contents.info.description);
}

void QnWorkbenchUpdateWatcher::showUpdateNotification(
    const nx::utils::SoftwareVersion& targetVersion,
    const nx::utils::Url& releaseNotesUrl,
    const QString& description)
{
    m_notifiedVersion = targetVersion;

    const auto current = commonModule()->engineVersion();
    const bool majorVersionChange = ((targetVersion.major() > current.major())
        || (targetVersion.minor() > current.minor()));

    const auto text = tr("%1 version available").arg(targetVersion.toString());
    const auto releaseNotesLink = makeHref(tr("Release Notes"), releaseNotesUrl);

    QStringList extras;
    if (majorVersionChange)
        extras.push_back(tr("Major issues have been fixed. Update is strongly recommended."));
    if (!description.isEmpty())
        extras.push_back(description);
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
        qnSettings->setIgnoredUpdateVersion(targetVersion);
        qnSettings->save();
    }

    if (result != QDialogButtonBox::Cancel)
        action(action::SystemUpdateAction)->trigger();
}

const UpdateContents& QnWorkbenchUpdateWatcher::getUpdateContents() const
{
    return m_private->updateContents;
}
