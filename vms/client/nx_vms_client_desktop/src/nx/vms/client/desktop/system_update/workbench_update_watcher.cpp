// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_update_watcher.h"

#include <QtCore/QTimer>
#include <QtGui/QAction>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/random.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/webview_widget.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/settings/system_specific_local_settings.h>
#include <nx/vms/client/desktop/style/webview_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_update/update_contents.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/update/tools.h>
#include <nx/vms/update/update_check.h>
#include <ui/common/notification_levels.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/util.h>

#include "server_update_tool.h"

using namespace std::chrono;

using namespace nx::vms::client::desktop;
using namespace nx::vms::common;

namespace {

constexpr int kTooLateDayOfWeek = Qt::Thursday;

constexpr milliseconds kUpdatePeriod = 1s;
constexpr milliseconds kCheckUpdatePeriod = 60min;
constexpr milliseconds kWaitForUpdateCheckFuture = 1ms;

} // namespace

namespace nx::vms::client::desktop {

struct WorkbenchUpdateWatcher::Private
{
    workbench::LocalNotificationsManager* notificationsManager = nullptr;
    UpdateContents updateContents;
    std::future<UpdateContents> updateCheck;
    QPointer<ServerUpdateTool> serverUpdateTool;
    bool connected = false;
};

WorkbenchUpdateWatcher::WorkbenchUpdateWatcher(WindowContext* windowContext, QObject* parent):
    QObject(parent),
    WindowContextAware(windowContext),
    m_updateStateTimer(this),
    m_checkLatestUpdateTimer(this),
    m_notifiedVersion(),
    m_updateAction(new CommandAction(tr("Updates"))),
    m_skipAction(new CommandAction(tr("Skip Version"), "16x16/Outline/skip.svg?primary=light16")),
    m_private(new Private())
{
    m_private->notificationsManager = windowContext->localNotificationsManager();
    NX_CRITICAL(m_private->notificationsManager);

    // FIXME: #amalov Recreate on system context change.
    m_private->serverUpdateTool = workbenchContext()->findInstance<ServerUpdateTool>();
    NX_ASSERT(m_private->serverUpdateTool);

    connect(windowContext, &WindowContext::beforeSystemChanged, this,
        [this]
        {
            if (m_private->serverUpdateTool)
                m_private->serverUpdateTool->onDisconnectFromSystem();
        });

    m_autoChecksEnabled = system()->globalSettings()->isUpdateNotificationsEnabled();

    connect(system()->globalSettings(), &SystemSettings::updateNotificationsChanged, this,
        [this]()
        {
            m_autoChecksEnabled = system()->globalSettings()->isUpdateNotificationsEnabled();
            syncState();
        });

    connect(&m_updateStateTimer, &QTimer::timeout,
        this, &WorkbenchUpdateWatcher::atUpdateCurrentState);

    // This is the timer for periodic checks for new update.
    connect(&m_checkLatestUpdateTimer, &QTimer::timeout,
        this, &WorkbenchUpdateWatcher::atStartCheckUpdate);

    auto checkInterval = ini().backgroupdUpdateCheckPeriodOverrideSec != 0
        ? std::chrono::seconds(ini().backgroupdUpdateCheckPeriodOverrideSec)
        : kCheckUpdatePeriod;

    m_checkLatestUpdateTimer.setInterval(checkInterval);
    m_updateStateTimer.setInterval(kUpdatePeriod);

    connect(m_private->notificationsManager,
        &workbench::LocalNotificationsManager::cancelRequested,
        this,
        [this]()
        {
            m_private->notificationsManager->remove(updateNotificationId);
            updateNotificationId = {};
        });

    connect(m_updateAction.data(),
        &CommandAction::triggered,
        this,
        [this]()
        {
            m_private->notificationsManager->remove(updateNotificationId);
            updateNotificationId = {};
            menu()->trigger(menu::SystemUpdateAction);
        });

    connect(m_skipAction.data(),
        &CommandAction::triggered,
        this,
        [this]()
        {
            system()->localSettings()->ignoredUpdateVersion = m_notifiedVersion;
            m_private->notificationsManager->remove(updateNotificationId);
            updateNotificationId = {};
        });

    connect(windowContext, &WindowContext::systemChanged, this,
        [this]()
        {
            if (!m_private->serverUpdateTool)
                return;

            auto systemId = system()->globalSettings()->localSystemId();
            if (systemId.isNull())
                systemId = system()->localSystemId();

            m_private->connected = !systemId.isNull();

            if (m_private->connected)
                m_private->serverUpdateTool->onConnectToSystem(systemId);

            syncState();
        });
}

WorkbenchUpdateWatcher::~WorkbenchUpdateWatcher() {}

void WorkbenchUpdateWatcher::syncState()
{
    if (m_private->connected && m_autoChecksEnabled && !m_updateStateTimer.isActive())
    {
        NX_VERBOSE(this, "Starting automatic checks for updates...");
        atStartCheckUpdate();
        m_updateStateTimer.start();
        m_checkLatestUpdateTimer.start();
    }
    else
    {
        NX_VERBOSE(this, "Stopping automatic checks for updates...");
        m_private->updateCheck = {};
        m_updateStateTimer.stop();
        m_checkLatestUpdateTimer.stop();
    }
}

nx::utils::SoftwareVersion WorkbenchUpdateWatcher::getMinimumComponentVersion() const
{
    nx::utils::SoftwareVersion minVersion = appContext()->version();
    for (const QnMediaServerResourcePtr& server: system()->resourcePool()->servers())
    {
        if (server->getVersion() < minVersion)
            minVersion = server->getVersion();
    }
    return minVersion;
}

void WorkbenchUpdateWatcher::atUpdateCurrentState()
{
    if (m_private->updateCheck.valid()
        && m_private->updateCheck.wait_for(kWaitForUpdateCheckFuture) == std::future_status::ready)
    {
        atCheckerUpdateAvailable(m_private->updateCheck.get());
    }
}

ServerUpdateTool* WorkbenchUpdateWatcher::getServerUpdateTool()
{
    return m_private->serverUpdateTool;
}

std::future<UpdateContents> WorkbenchUpdateWatcher::takeUpdateCheck()
{
    return std::move(m_private->updateCheck);
}

void WorkbenchUpdateWatcher::atStartCheckUpdate()
{
    // This signal will be removed when update check is complete.
    if (m_private->updateCheck.valid())
    {
        NX_VERBOSE(this, "atStartCheckUpdate() - there is already an active update check");
        return;
    }

    NX_VERBOSE(this, "atStartCheckUpdate()");

    const nx::utils::SoftwareVersion versionOverride(ini().autoUpdateCheckVersionOverride);
    if (versionOverride.isNull())
    {
        m_private->updateCheck = m_private->serverUpdateTool->checkForUpdate(
            update::LatestVmsVersionParams{getMinimumComponentVersion()});
    }
    else
    {
        m_private->updateCheck = m_private->serverUpdateTool->checkForUpdate(
            update::CertainVersionParams{versionOverride, getMinimumComponentVersion()});
    }
}

void WorkbenchUpdateWatcher::atCheckerUpdateAvailable(const UpdateContents& contents)
{
    if (!system()->globalSettings()->isUpdateNotificationsEnabled())
        return;

    m_private->updateContents = contents;

    // 'NoNewVersion' is considered invalid as well.
    if (!contents.info.isValid())
    {
        NX_VERBOSE(this, "atCheckerUpdateAvailable() update check returned an error %1", contents.error);
        return;
    }

    NX_VERBOSE(this, "atCheckerUpdateAvailable(%1)", contents.info.version.toString());

    // We have no access rights.
    if (!menu()->canTrigger(menu::SystemUpdateAction))
        return;

    auto targetVersion = contents.info.version;
    // User was already notified about this release.
    if (m_notifiedVersion == targetVersion)
        return;

    // Current version is greater or equal to latest.
    if (appContext()->version() >= targetVersion)
        return;

    // User is not interested in this update.
    if (system()->localSettings()->ignoredUpdateVersion() >= targetVersion)
        return;

    // Administrator disabled update notifications globally.
    if (!system()->globalSettings()->isUpdateNotificationsEnabled())
        return;

    // Do not show notifications near the end of the week or on our holidays.
    if (QDateTime::currentDateTime().date().dayOfWeek() >= kTooLateDayOfWeek)
        return;

    // TODO: Should inject this values inside UpdateInformation
    // Release date in msecs since epoch.
    const milliseconds releaseDate = contents.info.releaseDate;
    // Maximum days for release delivery.
    const int releaseDeliveryDays = contents.info.releaseDeliveryDays;

    const UpdateDeliveryInfo oldUpdateInfo = system()->localSettings()->updateDeliveryInfo();
    if (nx::utils::SoftwareVersion(oldUpdateInfo.version) != targetVersion
        || oldUpdateInfo.releaseDateMs != releaseDate
        || oldUpdateInfo.releaseDeliveryDays != releaseDeliveryDays)
    {
        NX_VERBOSE(this, "atCheckerUpdateAvailable(%1) - saving new update info and picking "
            "delivery date. old_version=\"%2\", old_delivery=%3:%4, new_delivery=%5:%6",
            contents.info.version.toString(), oldUpdateInfo.version,
            oldUpdateInfo.releaseDateMs, oldUpdateInfo.releaseDeliveryDays,
            releaseDate, releaseDeliveryDays);

        // New release was published or we decided to change the delivery period. Estimating the
        // new delivery date.
        constexpr int kHoursPerDay = 24;
        const milliseconds timeToDeliver = hours{
            nx::utils::random::number(0, releaseDeliveryDays * kHoursPerDay)};
        system()->localSettings()->updateDeliveryDate = (releaseDate + timeToDeliver).count();
    }
    system()->localSettings()->updateDeliveryInfo = contents.getUpdateDeliveryInfo();

    auto currentTimeFromEpoch = QDateTime::currentMSecsSinceEpoch();
    auto updateDeliveryDate = system()->localSettings()->updateDeliveryDate();
    // Update is postponed
    if (updateDeliveryDate > currentTimeFromEpoch)
        return;

    notifyUserAboutWorkbenchUpdate(targetVersion, contents.info.releaseNotesUrl, contents.info.description);
}

void WorkbenchUpdateWatcher::notifyUserAboutWorkbenchUpdate(
    const nx::utils::SoftwareVersion& targetVersion,
    const nx::Url& releaseNotesUrl,
    const QString& description)
{
    if (!updateNotificationId.isNull())
        return;

    if (!system()->accessController()->hasPowerUserPermissions())
        return;

    NX_VERBOSE(this, "Showing a notification about Workbench update feature.");

    m_notifiedVersion = targetVersion;

    const auto current = appContext()->version();
    const bool majorVersionChange = ((targetVersion.major > current.major)
        || (targetVersion.minor > current.minor)) && !description.isEmpty();

    const auto text = tr("%1 version available").arg(targetVersion.toString());

    const auto releaseNotesLink = common::html::link(tr("Release Notes..."), releaseNotesUrl);
    QStringList extras;
    if (majorVersionChange)
        extras.push_back(tr("Major issues have been fixed. Update is strongly recommended."));
    if (!description.isEmpty())
        extras.push_back(description);
    extras.push_back(releaseNotesLink);

    QString html = QString::fromLatin1(R"(
        <html>
        <body>%1</body>
        </html>
        )").arg(extras.join(common::html::kLineBreak));

    updateNotificationId = m_private->notificationsManager->add(
        tr("%1 Version is available").arg(toString(targetVersion)),
        html,
        true);

    m_private->notificationsManager->setLevel(
        updateNotificationId,
        majorVersionChange
            ? QnNotificationLevel::Value::ImportantNotification
            : QnNotificationLevel::Value::CommonNotification);

    m_private->notificationsManager->setIconPath(updateNotificationId, "20x20/Outline/download.svg");

    m_private->notificationsManager->setAction(updateNotificationId, m_updateAction);
    m_private->notificationsManager->setAdditionalAction(updateNotificationId, m_skipAction);
    m_private->notificationsManager->setTooltip(updateNotificationId, {});
}

const UpdateContents& WorkbenchUpdateWatcher::getUpdateContents() const
{
    return m_private->updateContents;
}

} // namespace nx::vms::client::desktop
