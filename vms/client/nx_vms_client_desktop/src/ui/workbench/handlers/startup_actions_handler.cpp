#include "startup_actions_handler.h"

#include <chrono>

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QAction>

#include <common/common_module.h>

#include <client/client_module.h>
#include <client/client_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>

#include <nx/vms/client/desktop/system_logon/data/logon_parameters.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/utils/mime_data.h>
#include <nx/vms/client/desktop/ini.h>

#include <ui/graphics/items/controls/time_slider.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_state_manager.h>
#include <ui/workbench/workbench_welcome_screen.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/workbench_navigator.h>

#include <watchers/cloud_status_watcher.h>

#include <utils/common/credentials.h>
#include <utils/common/synctime.h>

#include <api/helpers/layout_id_helper.h>

#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>

namespace {

using namespace std::chrono;
using namespace std::chrono_literals;

/** In ACS mode set timeline window to 5 minutes. */
static const milliseconds kAcsModeTimelineWindowSize = 5min;

/**
 * Search for layout by its name. Checks only layouts, available to the given user.
 */
QnLayoutResourcePtr findAvailableLayoutByName(const QnUserResourcePtr& user, QString layoutName)
{
    if (!user->commonModule() || !user->resourcePool())
        return {};

    const auto resourcePool = user->resourcePool();
    const auto accessProvider = user->commonModule()->resourceAccessProvider();

    const auto filter =
        [&](const QnLayoutResourcePtr& layout)
        {
            return layout->getName() == layoutName
                && accessProvider->hasAccess(user, layout);
        };

    const auto layouts = user->resourcePool()->getResources<QnLayoutResource>(filter);
    return layouts.empty() ? QnLayoutResourcePtr() : layouts.first();
}

} // namespace

namespace nx::vms::client::desktop {

using namespace ui::action;

struct StartupActionsHandler::Private
{
    bool delayedDropGuard = false;

    // List of serialized resources that are to be dropped on the scene once the user logs in.
    struct
    {
        QByteArray raw;
        QList<QnUuid> resourceIds;
        qint64 timeStampMs = 0;
        QString layoutRef;

        bool empty() const
        {
            return raw.isEmpty() && resourceIds.empty() && layoutRef.isEmpty();
        }
    } delayedDrops;
};

StartupActionsHandler::StartupActionsHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private())
{
    connect(action(ProcessStartupParametersAction), &QAction::triggered, this,
        &StartupActionsHandler::handleStartupParameters);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        &StartupActionsHandler::handleUserLoggedIn);
}

StartupActionsHandler::~StartupActionsHandler()
{
}

void StartupActionsHandler::handleUserLoggedIn(const QnUserResourcePtr& user)
{
    if (!user)
        return;

    // We should not change state when using "Open in New Window". Otherwise workbench will be
    // cleared here even if no state is saved.
    if (d->delayedDrops.empty() && qnRuntime->isDesktopMode())
        context()->instance<QnWorkbenchStateManager>()->restoreState();

    // Sometimes we get here when 'New Layout' has already been added. But all user's layouts must
    // be created AFTER this method. Otherwise the user will see uncreated layouts in layout
    // selection menu. As temporary workaround we can just remove that layouts.
    // TODO: #dklychkov Do not create new empty layout before this method end.
    // See: LayoutsHandler::at_openNewTabAction_triggered()
    if (user && !qnRuntime->isAcsMode())
    {
        for (const QnLayoutResourcePtr& layout: resourcePool()->getResourcesByParentId(
                 user->getId()).filtered<QnLayoutResource>())
        {
            if (layout->hasFlags(Qn::local) && !layout->isFile())
                resourcePool()->removeResource(layout);
        }
    }

    if (workbench()->layouts().empty())
        menu()->trigger(OpenNewTabAction);

    submitDelayedDrops();
}

void StartupActionsHandler::submitDelayedDrops()
{
    if (d->delayedDropGuard)
        return;

    if (d->delayedDrops.empty())
        return;

    // Delayed drops actual only for logged-in users.
    if (!context()->user())
        return;

    QScopedValueRollback<bool> guard(d->delayedDropGuard, true);

    QnResourceList resources;
    nx::vms::api::LayoutTourDataList tours;

    if (const auto layoutRef = d->delayedDrops.layoutRef; !layoutRef.isEmpty())
    {
        auto layout = layout_id_helper::findLayoutByFlexibleId(resourcePool(), layoutRef);
        if (!layout)
            layout = findAvailableLayoutByName(context()->user(), layoutRef);

        if (layout)
            resources.append(layout);
    }

    const auto mimeData = MimeData::deserialized(d->delayedDrops.raw, resourcePool());

    for (const auto& tour: layoutTourManager()->tours(mimeData.entities()))
        tours.push_back(tour);

    resources.append(mimeData.resources());
    if (!resources.isEmpty())
        resourcePool()->addNewResources(resources);

    resources.append(resourcePool()->getResourcesByIds(d->delayedDrops.resourceIds));
    const auto timeStampMs = d->delayedDrops.timeStampMs;

    d->delayedDrops = {};

    if (resources.empty() && tours.empty())
        return;

    workbench()->clear();

    if (qnRuntime->isAcsMode())
    {
        handleAcsModeResources(resources, timeStampMs);
        return;
    }

    if (!resources.empty())
    {
        Parameters parameters(resources);
        if (timeStampMs != 0)
            parameters.setArgument(Qn::ItemTimeRole, timeStampMs);
        menu()->trigger(OpenInNewTabAction, parameters);
    }

    for (const auto& tour: tours)
        menu()->trigger(ReviewLayoutTourAction, {Qn::UuidRole, tour.id});
}

void StartupActionsHandler::handleStartupParameters()
{
    const auto startupParameters = menu()->currentParameters(sender())
        .argument<QnStartupParameters>(Qn::StartupParametersRole);

    // Process input files.
    bool haveInputFiles = false;
    if (auto window = qobject_cast<MainWindow*>(mainWindow()))
    {
        NX_ASSERT(window);

        bool skipArg = true;
        // TODO: Apply this to unparsed parameters only.
        for (const auto& arg: qApp->arguments())
        {
            if (!skipArg)
                haveInputFiles |= window->handleOpenFile(arg);
            skipArg = false;
        }
    }

    if (const auto welcomeScreen = mainWindow()->welcomeScreen())
        welcomeScreen->setVisibleControls(true);

    connectToCloudIfNeeded(startupParameters);
    const bool connectingToSystem = connectToSystemIfNeeded(startupParameters, haveInputFiles);

    if (!startupParameters.videoWallGuid.isNull())
    {
        NX_ASSERT(connectingToSystem);
        menu()->trigger(DelayedOpenVideoWallItemAction, Parameters()
            .withArgument(Qn::VideoWallGuidRole, startupParameters.videoWallGuid)
            .withArgument(Qn::VideoWallItemGuidRole, startupParameters.videoWallItemGuid));
    }

    if (!startupParameters.instantDrop.isEmpty())
    {
        const auto raw = QByteArray::fromBase64(startupParameters.instantDrop.toLatin1());
        const auto mimeData = MimeData::deserialized(raw, resourcePool());
        const auto resources = mimeData.resources();
        resourcePool()->addNewResources(resources);
        handleInstantDrops(resources);
    }

    if (!startupParameters.delayedDrop.isEmpty())
    {
        if (NX_ASSERT(connectingToSystem))
            d->delayedDrops.raw = QByteArray::fromBase64(startupParameters.delayedDrop.toLatin1());
    }

    if (startupParameters.customUri.isValid())
    {
        if (NX_ASSERT(connectingToSystem))
        {
            d->delayedDrops.resourceIds = startupParameters.customUri.resourceIds();
            d->delayedDrops.timeStampMs = startupParameters.customUri.timestamp();
        }
    }

    if (!startupParameters.layoutRef.isEmpty())
    {
        if (NX_ASSERT(connectingToSystem))
            d->delayedDrops.layoutRef = startupParameters.layoutRef;
    }

    submitDelayedDrops();

    // Show beta version warning message for the main instance only.
    const bool showBetaWarning = nx::utils::AppInfo::beta()
        && !startupParameters.allowMultipleClientInstances
        && !ini().developerMode;

    if (showBetaWarning)
        menu()->triggerIfPossible(BetaVersionMessageAction);

    if (ini().developerMode || ini().profilerMode)
        menu()->trigger(ShowFpsAction);
}

void StartupActionsHandler::handleInstantDrops(const QnResourceList& resources)
{
    if (resources.empty())
        return;

    workbench()->clear();
    const bool dropped = menu()->triggerIfPossible(OpenInNewTabAction, resources);
    if (dropped)
        action(ResourcesModeAction)->setChecked(true);

    // Security check - just in case.
    if (workbench()->layouts().empty())
        menu()->trigger(OpenNewTabAction);
}

void StartupActionsHandler::handleAcsModeResources(
    const QnResourceList& resources,
    const qint64 timeStampMs)
{
    const auto maxTime = milliseconds(qnSyncTime->currentMSecsSinceEpoch());

    milliseconds windowStart(timeStampMs);
    windowStart -= kAcsModeTimelineWindowSize / 2;

    // Adjust period if start time was not passed or passed too near to live.
    if (windowStart.count() < 0 || windowStart + kAcsModeTimelineWindowSize > maxTime)
        windowStart = maxTime - kAcsModeTimelineWindowSize;

    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setId(QnUuid::createUuid());
    layout->setParentId(context()->user()->getId());
    layout->setCellSpacing(0);
    resourcePool()->addResource(layout);

    const auto wlayout = new QnWorkbenchLayout(layout, this);
    workbench()->addLayout(wlayout);
    workbench()->setCurrentLayout(wlayout);

    menu()->trigger(OpenInCurrentLayoutAction, resources);

    // Set range to maximum allowed value, so we will not constrain window by any values.
    auto timeSlider = context()->navigator()->timeSlider();
    timeSlider->setTimeRange(milliseconds(0), maxTime);
    timeSlider->setWindow(windowStart, windowStart + kAcsModeTimelineWindowSize, false);

    navigator()->setPosition(timeStampMs * 1000);
}

bool StartupActionsHandler::connectUsingCustomUri(const nx::vms::utils::SystemUri& uri)
{
    using namespace nx::vms::utils;

    if (!uri.isValid())
        return false;

    if (uri.clientCommand() != SystemUri::ClientCommand::Client)
        return false;

    QString systemId = uri.systemId();
    if (systemId.isEmpty())
        return false;

    SystemUri::Auth auth = uri.authenticator();
    const nx::vms::common::Credentials credentials(auth.user, auth.password);

    const bool systemIsCloud = !QnUuid::fromStringSafe(systemId).isNull();
    if (systemIsCloud)
    {
        qnClientModule->cloudStatusWatcher()->setCredentials(credentials, true);
        NX_DEBUG(this, "Custom URI: System is cloud, connecting to the cloud first");
    }

    auto systemUrl = nx::utils::Url::fromUserInput(systemId);
    systemUrl.setScheme("https");
    NX_DEBUG(this, "Custom URI: Connecting to the system %1", systemUrl.toString());

    systemUrl.setUserName(auth.user);
    systemUrl.setPassword(auth.password);

    LogonParameters parameters(systemUrl);
    parameters.force = true;
    parameters.storeSession = false;
    parameters.secondaryInstance = true;
    menu()->trigger(ConnectAction, Parameters().withArgument(Qn::LogonParametersRole, parameters));
    return true;
}

bool StartupActionsHandler::connectUsingCommandLineAuth(
    const QnStartupParameters& startupParameters)
{
    // Set authentication parameters from the command line.
    nx::utils::Url serverUrl = startupParameters.parseAuthenticationString();

    // TODO: #refactor System URI to support videowall
    if (!startupParameters.videoWallGuid.isNull())
    {
        if (!NX_ASSERT(serverUrl.isValid()))
            return false;

        serverUrl.setUserName(startupParameters.videoWallGuid.toString());
    }

    if (!serverUrl.isValid())
        return false;

    LogonParameters parameters(serverUrl);
    parameters.force = true;
    parameters.storeSession = false;
    parameters.secondaryInstance = true;
    menu()->trigger(ConnectAction, Parameters().withArgument(Qn::LogonParametersRole, parameters));
    return true;
}

bool StartupActionsHandler::connectToSystemIfNeeded(
    const QnStartupParameters& startupParameters,
    bool haveInputFiles)
{
    // If no input files were supplied --- open welcome page.
    // Do not try to connect in the following cases:
    // * client was not connected and user clicked "Open in new window"
    // * user opened exported exe-file
    // Otherwise we should try to connect or show welcome page.

    // TODO: #GDM Separate this mess to certain scenarios. Videowall, "Open new Window", etc.
    if (connectUsingCustomUri(startupParameters.customUri))
        return true;

    // A local file is being opened.
    if (!startupParameters.instantDrop.isEmpty() || haveInputFiles)
        return false;

    if (connectUsingCommandLineAuth(startupParameters))
        return true;

    // Attempt auto-login, if needed.
    if (qnSettings->autoLogin() && qnSettings->lastUsedConnection().url.isValid()
        && !qnSettings->lastUsedConnection().localId.isNull())
    {
        menu()->trigger(ConnectAction,
            Parameters().withArgument(Qn::LogonParametersRole, LogonParameters()));
        return true;
    }

    return false;
}

bool StartupActionsHandler::connectToCloudIfNeeded(const QnStartupParameters& startupParameters)
{
    using namespace nx::vms::utils;

    const auto uri = startupParameters.customUri;
    if (!uri.isValid())
        return false;

    if (uri.clientCommand() != SystemUri::ClientCommand::LoginToCloud)
        return false;

    SystemUri::Auth auth = uri.authenticator();
    const nx::vms::common::Credentials credentials(auth.user, auth.password);

    NX_DEBUG(this, "Custom URI: Connecting to cloud as %1", auth.user);
    qnClientModule->cloudStatusWatcher()->setCredentials(credentials, true);
    return true;
}

} // namespace nx::vms::client::desktop
