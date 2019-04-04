#include "workbench_update_watcher.h"

#include <QtCore/QTimer>
#include <QtGui/QDesktopServices>
#include <QtWebKitWidgets/QWebView>
#include <QtWidgets/QAction>
#include <QtWidgets/QPushButton>

#include <api/global_settings.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <client/client_settings.h>

#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/system_update/update_contents.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/style/webview_style.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/html.h>

#include <utils/common/app_info.h>
#include <utils/common/util.h>
#include <nx/utils/random.h>
#include <nx/utils/guarded_callback.h>

#include <nx/update/update_information.h>
#include <nx/update/update_check.h>

#include "server_update_tool.h"

using namespace nx::vms::client::desktop::ui;
using UpdateContents = nx::update::UpdateContents;

namespace {
    constexpr int kUpdatePeriodMSec = 10000; //< 10 seconds.
    constexpr int kHoursPerDay = 24;
    constexpr int kMinutesPerHour = 60;
    constexpr int kSecsPerMinute = 60;
    constexpr int kTooLateDayOfWeek = Qt::Thursday;
    const auto kWaitForUpdateCheck = std::chrono::milliseconds(1);
} // anonymous namespace

namespace nx::vms::client::desktop {

struct WorkbenchUpdateWatcher::Private
{
    UpdateContents updateContents;
    std::future<UpdateContents> m_updateCheck;
    std::shared_ptr<ServerUpdateTool> m_serverUpdateTool;
};

WorkbenchUpdateWatcher::WorkbenchUpdateWatcher(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_updateStateTimer(this),
    m_notifiedVersion(),
    m_private(new Private())
{
    m_private->m_serverUpdateTool.reset(new ServerUpdateTool(this));
    m_autoChecksEnabled = qnGlobalSettings->isUpdateNotificationsEnabled();

    connect(qnGlobalSettings, &QnGlobalSettings::updateNotificationsChanged, this,
        [this]()
        {
            m_autoChecksEnabled = qnGlobalSettings->isUpdateNotificationsEnabled();
            syncState();
        });

    connect(&m_updateStateTimer, &QTimer::timeout,
        this, &WorkbenchUpdateWatcher::atUpdateCurrentState);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        [this](const QnUserResourcePtr &user)
        {
            m_userLoggedIn = user != nullptr;
            syncState();
        });
}

WorkbenchUpdateWatcher::~WorkbenchUpdateWatcher() {}

void WorkbenchUpdateWatcher::syncState()
{
    if (m_userLoggedIn && m_autoChecksEnabled && !m_updateStateTimer.isActive())
    {
        NX_VERBOSE(this, "syncState() - starting automatic checks for updates");
        atStartCheckUpdate();
        m_updateStateTimer.start(kUpdatePeriodMSec);
    }

    if ((!m_userLoggedIn || !m_autoChecksEnabled) && m_updateStateTimer.isActive())
    {
        NX_VERBOSE(this, "syncState() - stopping automatic checks for updates");
        m_updateStateTimer.stop();
    }
}

void WorkbenchUpdateWatcher::atUpdateCurrentState()
{
    NX_ASSERT(m_private);
    if (m_private->m_updateCheck.valid()
        && m_private->m_updateCheck.wait_for(kWaitForUpdateCheck) == std::future_status::ready)
    {
        atCheckerUpdateAvailable(m_private->m_updateCheck.get());
    }
}

std::shared_ptr<ServerUpdateTool> WorkbenchUpdateWatcher::getServerUpdateTool()
{
    NX_ASSERT(m_private);
    return m_private->m_serverUpdateTool;
}

std::future<UpdateContents> WorkbenchUpdateWatcher::takeUpdateCheck()
{
    NX_ASSERT(m_private);
    return std::move(m_private->m_updateCheck);
}

void WorkbenchUpdateWatcher::atStartCheckUpdate()
{
    // This signal will be removed when update check is complete.
    if (m_private->m_updateCheck.valid())
        return;
    QString updateUrl = qnSettings->updateFeedUrl();
    NX_ASSERT(!updateUrl.isEmpty());

    QString changesetOverride = ini().autoUpdatesCheckChangesetOverride;
    if (changesetOverride.isEmpty())
    {
        m_private->m_updateCheck = m_private->m_serverUpdateTool->checkLatestUpdate(updateUrl);
    }
    else
    {
        m_private->m_updateCheck = m_private->m_serverUpdateTool->checkSpecificChangeset(
            updateUrl, changesetOverride);
    }
}

void WorkbenchUpdateWatcher::atCheckerUpdateAvailable(const UpdateContents& contents)
{
    NX_INFO(this, "atCheckerUpdateAvailable(%1)", contents.getVersion().toString());
    if (!qnGlobalSettings->isUpdateNotificationsEnabled())
        return;

    m_private->m_updateCheck = std::future<UpdateContents>();
    m_private->updateContents = contents;

    // 'NoNewVersion' is considered invalid as well.
    if (!contents.info.isValid())
        return;

    // We are not interested in updates right now.
    //if (!m_checkUpdateTimer.isActive())
    //    return;

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

void WorkbenchUpdateWatcher::showUpdateNotification(
    const nx::utils::SoftwareVersion& targetVersion,
    const nx::utils::Url& releaseNotesUrl,
    const QString& description)
{
    m_notifiedVersion = targetVersion;

    const auto current = commonModule()->engineVersion();
    const bool majorVersionChange = ((targetVersion.major() > current.major())
        || (targetVersion.minor() > current.minor())) && !description.isEmpty();

    const auto text = tr("%1 version available").arg(targetVersion.toString());
    const auto releaseNotesLink = makeHref(tr("Release Notes"), releaseNotesUrl);

    QStringList extras;
    if (majorVersionChange)
        extras.push_back(tr("Major issues have been fixed. Update is strongly recommended."));
    if (!description.isEmpty())
        extras.push_back(description);
    extras.push_back(releaseNotesLink);

    QnMessageBox messageBox(QnMessageBoxIcon::Information,
        text, "",
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        mainWindowWidget());

    QString cssStyle = NxUi::generateCssStyle();
    QString htmlBody= extras.join("<br>");
    QString html = QString::fromLatin1(R"(
        <html>
        <head>
            <style media="screen" type="text/css">%1</style>
        </head>
        <body>%2</body>
        </html>
        )").arg(cssStyle, htmlBody);

    auto view = new QWebView(&messageBox);
    NxUi::setupWebViewStyle(view, NxUi::WebViewStyle::eula);
    view->setHtml(html);
    // QWebView has weird sizeHint. We should manually adjust its size to make it look good.
    view->setFixedWidth(360);
    if (description.isEmpty())
        view->setFixedHeight(20);
    else
        view->setFixedHeight(320);
    view->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
    // Setting up a policy for link redirection. We should not open release notes right here.
    auto page = view->page();
    page->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    connect(page, &QWebPage::linkClicked, this,
        [](const QUrl& url)
        {
            QDesktopServices::openUrl(url);
        });
    messageBox.addCustomWidget(view, QnMessageBox::Layout::AfterMainLabel);
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

const UpdateContents& WorkbenchUpdateWatcher::getUpdateContents() const
{
    return m_private->updateContents;
}

} // namespace nx::vms::client::desktop
