// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "startup_actions_handler.h"

#include <chrono>

#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>
#include <QtQml/QQmlEngine>
#include <QtWidgets/QAction>

#include <api/helpers/layout_id_helper.h>
#include <client/client_module.h>
#include <client/client_runtime_settings.h>
#include <client/client_settings.h>
#include <client/client_startup_parameters.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>
#include <helpers/system_helpers.h> //< Saved credentials helpers.
#include <nx/build_info.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/trace/trace.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/desktop/debug_utils/utils/performance_monitor.h>
#include <nx/vms/client/desktop/director/director.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_logon/data/logon_parameters.h>
#include <nx/vms/client/desktop/system_logon/ui/welcome_screen.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/utils/mime_data.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/statistics/modules/certificate_statistics_module.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_state_manager.h>
#include <utils/common/synctime.h>
#include <watchers/cloud_status_watcher.h>

namespace {

using namespace std::chrono;

/** In ACS mode set timeline window to 5 minutes. */
constexpr auto kAcsModeTimelineWindowSize = 5min;
constexpr auto kDelayedLogonTimeout = 30s;

/**
 * Search for layout by its name. Checks only layouts, available to the given user.
 */
QnLayoutResourcePtr findAvailableLayoutByName(const QnUserResourcePtr& user, QString layoutName)
{
    if (!user->commonModule() || !user->resourcePool())
        return {};

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
    QnWorkbenchContext* const context;

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

    std::unique_ptr<LogonParameters> delayedLogonParameters;
    std::unique_ptr<QTimer> delayedLogonTimer;

    Private(QnWorkbenchContext* context):
        context(context)
    {
    }

    void onCloudSystemsChanged(const QnCloudSystemList& systems)
    {
        if (!delayedLogonParameters)
            return;

        if (!systems.empty() && qnCloudStatusWatcher->status() == QnCloudStatusWatcher::Online)
        {
            context->menu()->trigger(
                ConnectAction,
                Parameters().withArgument(Qn::LogonParametersRole, *delayedLogonParameters));

            // Preloader will be hidden by connect action.
            resetDelayedLogon(/*hidePreloader*/ false);
        }
        else
        {
            //FIXME: #amalov Add error message in case of invalid status.
            resetDelayedLogon(/*hidePreloader*/ true);
        }
    }

    void setDelayedLogon(std::unique_ptr<LogonParameters> logonParameters)
    {
        delayedLogonParameters = std::move(logonParameters);

        if (delayedLogonParameters)
        {
            context->mainWindow()->welcomeScreen()->setGlobalPreloaderVisible(true);

            delayedLogonTimer = std::make_unique<QTimer>();
            delayedLogonTimer->setSingleShot(true);
            delayedLogonTimer->callOnTimeout(
                [this](){ resetDelayedLogon(/*hidePreloader*/ true); });
            delayedLogonTimer->start(kDelayedLogonTimeout);
        }
    }

    void resetDelayedLogon(bool hidePreloader)
    {
        delayedLogonTimer.reset();
        delayedLogonParameters.reset();

        if (hidePreloader)
            context->mainWindow()->welcomeScreen()->setGlobalPreloaderVisible(false);
    }
};

StartupActionsHandler::StartupActionsHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(context()))
{
    connect(action(ProcessStartupParametersAction), &QAction::triggered, this,
        &StartupActionsHandler::handleStartupParameters);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        &StartupActionsHandler::handleUserLoggedIn);

    // Delayed system connection may require information from qnSystemFinder,
    // so let it process signal first, by queuing following connection.
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::cloudSystemsChanged, this,
        [this](const QnCloudSystemList &systems) { d->onCloudSystemsChanged(systems); },
        Qt::QueuedConnection);
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
        workbench()->applyLoadedState();

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

    if (connectingToSystem && !startupParameters.delayedDrop.isEmpty())
    {
        d->delayedDrops.raw = QByteArray::fromBase64(startupParameters.delayedDrop.toLatin1());
    }

    if (connectingToSystem && startupParameters.customUri.isValid())
    {
        d->delayedDrops.resourceIds = startupParameters.customUri.resourceIds();
        d->delayedDrops.timeStampMs = startupParameters.customUri.timestamp();
    }

    if (connectingToSystem && !startupParameters.layoutRef.isEmpty())
    {
        d->delayedDrops.layoutRef = startupParameters.layoutRef;
    }

    submitDelayedDrops();

    // Show beta version for all publication types except releases and montly patches.
    const bool nonPublicVersion =
        nx::build_info::publicationType() != nx::build_info::PublicationType::patch
        && nx::build_info::publicationType() != nx::build_info::PublicationType::release;

    // Show beta version warning message for the main instance only.
    const bool showBetaMessages = !startupParameters.allowMultipleClientInstances
        && !ini().developerMode;

    if (nonPublicVersion && showBetaMessages)
        menu()->triggerIfPossible(BetaVersionMessageAction);

    if (showBetaMessages)
        menu()->triggerIfPossible(BetaUpgradeWarningAction);

    if (ini().developerMode || ini().profilerMode)
        menu()->trigger(ShowFpsAction);

    if (!startupParameters.scriptFile.isEmpty())
        handleScriptFile(startupParameters.scriptFile);

    const auto traceFile = startupParameters.traceFile.isEmpty()
        ? qgetenv("NX_TRACE_FILE").toStdString()
        : startupParameters.traceFile.toStdString();

    if (!traceFile.empty())
    {
        nx::utils::trace::Log::enable(traceFile);
        qnClientModule->performanceMonitor()->setEnabled(true);
    }
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
    layout->setIdUnsafe(QnUuid::createUuid());
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

void StartupActionsHandler::handleScriptFile(const QString& path)
{
    auto engine = qnClientCoreModule->mainQmlEngine();
    Director::instance()->setupJSEngine(engine);

    QFile scriptFile(path);
    if (!scriptFile.open(QIODevice::ReadOnly))
    {
        NX_ERROR(this, "Script file not found : %1", path);
        return;
    }

    QTextStream stream(&scriptFile);
    QString contents = stream.readAll();
    scriptFile.close();

    // TODO: Introduce modules support after switching to Qt 5.12+
    QJSValue result = engine->evaluate(contents, path);
    if (result.isError())
    {
        NX_ERROR(this, "Uncaught exception at %1:%2 : %3",
            result.property("fileName").toString(),
            result.property("lineNumber").toInt(),
            result.toString());
        return;
    }
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

    const SystemUri::Auth& auth = uri.authenticator();

    const auto systemUrl = nx::utils::Url::fromUserInput(systemId);
    nx::network::SocketAddress address(systemUrl.host(), systemUrl.port());
    NX_DEBUG(this, "Custom URI: Connecting to the system %1", address);

    nx::network::http::Credentials credentials(
        auth.user.toStdString(),
        nx::network::http::PasswordAuthToken(auth.password.toStdString()));

    LogonParameters parameters({address, credentials});
    parameters.storeSession = false;
    parameters.secondaryInstance = true;
    parameters.connectScenario = ConnectScenario::connectUsingCommand;

    if (uri.hasCloudSystemId())
        parameters.connectionInfo.userType = nx::vms::api::UserType::cloud;

    if (parameters.connectionInfo.userType == nx::vms::api::UserType::cloud
        && parameters.connectionInfo.credentials.authToken.empty())
    {
        d->setDelayedLogon(std::make_unique<LogonParameters>(std::move(parameters)));
    }
    else
    {
        menu()->trigger(
            ConnectAction, Parameters().withArgument(Qn::LogonParametersRole, parameters));
    }
    return true;
}

bool StartupActionsHandler::connectUsingCommandLineAuth(
    const QnStartupParameters& startupParameters)
{
    // Set authentication parameters from the command line.
    core::ConnectionInfo connectionInfo = startupParameters.parseAuthenticationString();
    if (connectionInfo.address.isNull())
        return false;

    // Fix compatibility mode command line from old versions.
    const bool isCloudSystem = nx::network::SocketGlobals::addressResolver().isCloudHostname(
        connectionInfo.address.toString());
    if (isCloudSystem && connectionInfo.credentials.authToken.isPassword())
        connectionInfo.userType = nx::vms::api::UserType::cloud;

    NX_DEBUG(this, "Connecting to %1 using command line auth", connectionInfo.address);

    LogonParameters parameters(std::move(connectionInfo));
    parameters.storeSession = false;
    parameters.secondaryInstance = true;
    parameters.connectScenario = ConnectScenario::connectUsingCommand;

    if (parameters.connectionInfo.userType == nx::vms::api::UserType::cloud)
    {
        d->setDelayedLogon(std::make_unique<LogonParameters>(std::move(parameters)));
        return true;
    }

    if (!startupParameters.videoWallGuid.isNull())
    {
        parameters.connectionInfo.credentials.authToken =
            nx::network::http::VideoWallAuthToken(startupParameters.videoWallGuid);
    }

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

    // TODO: #sivanov Separate to certain scenarios. Videowall, "Open new Window", etc.
    if (connectUsingCustomUri(startupParameters.customUri))
        return true;

    // A local file is being opened.
    if (!startupParameters.instantDrop.isEmpty() || haveInputFiles)
        return false;

    if (connectUsingCommandLineAuth(startupParameters))
        return true;

    // Attempt auto-login, if needed.
    return attemptAutoLogin();
}

bool StartupActionsHandler::attemptAutoLogin()
{
    if (!qnSettings->autoLogin() || !qnSettings->saveCredentialsAllowed())
        return false;

    const auto [url, systemId] = qnSettings->lastUsedConnection();

    if (!url.isValid() || systemId.isNull())
        return false;

    auto storedCredentials = nx::vms::client::core::CredentialsManager::credentials(
        systemId,
        url.userName().toStdString());

    LogonParameters logonParams;
    logonParams.connectScenario = ConnectScenario::autoConnect;
    auto& connectionInfo = logonParams.connectionInfo;

    if (storedCredentials)
    {
        connectionInfo.address = nx::network::SocketAddress::fromUrl(url);
        connectionInfo.credentials = std::move(*storedCredentials);

        NX_DEBUG(this, "Auto-login to the server %1", connectionInfo.address);
        menu()->trigger(ConnectAction, Parameters()
            .withArgument(Qn::LogonParametersRole, logonParams));

        return true;
    }
    else if (qnCloudStatusWatcher->status() != QnCloudStatusWatcher::LoggedOut)
    {
        connectionInfo.userType = nx::vms::api::UserType::cloud;
        connectionInfo.address = nx::network::SocketAddress(systemId.toSimpleStdString());

        NX_DEBUG(this, "Auto-login to the Cloud system %1", connectionInfo.address);
        d->setDelayedLogon(std::make_unique<LogonParameters>(std::move(logonParams)));

        return true;
    }

    return false;
}

bool StartupActionsHandler::connectToCloudIfNeeded(const QnStartupParameters& startupParameters)
{
    nx::vms::client::core::CloudAuthData authData;

    const auto& uri = startupParameters.customUri;
    if (uri.isValid()
        && (uri.hasCloudSystemId()
            || uri.clientCommand() == nx::vms::utils::SystemUri::ClientCommand::LoginToCloud))
    {
        authData.authorizationCode = uri.authenticator().authCode.toStdString();
        NX_DEBUG(
            this, "Received authorization code, length: %1", authData.authorizationCode.size());
    }

    if (authData.empty())
    {
        authData = nx::vms::client::core::helpers::loadCloudCredentials();
        NX_DEBUG(
            this, "Loaded auth data, refresh token length: %1", authData.refreshToken.length());
    }

    if (authData.empty())
        return false;

    NX_DEBUG(this, "Connecting to cloud as %1", authData.credentials.username);
    qnClientModule->cloudStatusWatcher()->setInitialAuthData(authData);
    return true;
}

} // namespace nx::vms::client::desktop
