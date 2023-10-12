// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "startup_actions_handler.h"

#include <chrono>

#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtQml/QQmlEngine>

#include <api/common_message_processor.h>
#include <api/helpers/layout_id_helper.h>
#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/build_info.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/trace/trace.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/network/logon_data_helpers.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/debug_utils/utils/performance_monitor.h>
#include <nx/vms/client/desktop/director/director.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/data/logon_data.h>
#include <nx/vms/client/desktop/system_logon/ui/welcome_screen.h>
#include <nx/vms/client/desktop/testkit/testkit.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/utils/mime_data.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/system_context.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/statistics/modules/certificate_statistics_module.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_state_manager.h>
#include <utils/common/synctime.h>

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
    if (!NX_ASSERT(user->systemContext()))
        return {};

    const auto accessManager = user->systemContext()->resourceAccessManager();

    const auto filter =
        [&](const QnLayoutResourcePtr& layout)
        {
            return layout->getName() == layoutName
                && accessManager->hasPermission(user, layout, Qn::ReadPermission);
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

    std::unique_ptr<LogonData> delayedLogonParameters;
    std::unique_ptr<QTimer> delayedLogonTimer;

    Private(QnWorkbenchContext* context):
        context(context)
    {
    }

    void onCloudSystemsChanged(const QnCloudSystemList& systems)
    {
        if (!delayedLogonParameters)
            return;

        if (!systems.empty() && qnCloudStatusWatcher->status() == core::CloudStatusWatcher::Online)
        {
            // Patch logon data for autoconnect scenario.
            if (delayedLogonParameters->connectScenario == ConnectScenario::autoConnect
                && delayedLogonParameters->userType == nx::vms::api::UserType::cloud)
            {
                if (const auto logonData = nx::vms::client::core::cloudLogonData(
                    QString::fromStdString(delayedLogonParameters->address.address.toString())))
                {
                    delayedLogonParameters->address = logonData->address;
                    delayedLogonParameters->expectedServerId = logonData->expectedServerId;
                    delayedLogonParameters->expectedServerVersion =
                        logonData->expectedServerVersion;
                    delayedLogonParameters->expectedCloudSystemId =
                        logonData->expectedCloudSystemId;
                }
            }

            context->menu()->trigger(
                ConnectAction,
                Parameters().withArgument(Qn::LogonDataRole, *delayedLogonParameters));

            // Preloader will be hidden by connect action.
            resetDelayedLogon(/*hidePreloader*/ false);
        }
        else
        {
            //FIXME: #amalov Add error message in case of invalid status.
            resetDelayedLogon(/*hidePreloader*/ true);
        }
    }

    void setDelayedLogon(std::unique_ptr<LogonData> logonParameters)
    {
        delayedLogonParameters = std::move(logonParameters);

        if (delayedLogonParameters)
        {
            if (auto welcomeScreen = context->mainWindow()->welcomeScreen())
                welcomeScreen->setGlobalPreloaderVisible(true);

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
        {
            if (auto welcomeScreen = context->mainWindow()->welcomeScreen())
                welcomeScreen->setGlobalPreloaderVisible(false);
        }
    }
};

StartupActionsHandler::StartupActionsHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(context()))
{
    connect(action(ProcessStartupParametersAction), &QAction::triggered, this,
        &StartupActionsHandler::handleStartupParameters);

    connect(context()->messageProcessor(), &QnCommonMessageProcessor::initialResourcesReceived,
        this, &StartupActionsHandler::handleConnected);

    // Delayed system connection may require information from qnSystemFinder,
    // so let it process signal first, by queuing following connection.
    connect(qnCloudStatusWatcher, &core::CloudStatusWatcher::cloudSystemsChanged, this,
        [this](const QnCloudSystemList &systems) { d->onCloudSystemsChanged(systems); },
        Qt::QueuedConnection);
}

StartupActionsHandler::~StartupActionsHandler()
{
}

void StartupActionsHandler::handleConnected()
{
    if (!context()->user())
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
    if (!qnRuntime->isAcsMode())
    {
        for (const QnLayoutResourcePtr& layout: resourcePool()->getResourcesByParentId(
            context()->user()->getId()).filtered<QnLayoutResource>())
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
    if (const auto layoutRef = d->delayedDrops.layoutRef; !layoutRef.isEmpty())
    {
        auto layout = layout_id_helper::findLayoutByFlexibleId(resourcePool(), layoutRef);
        if (!layout)
            layout = findAvailableLayoutByName(context()->user(), layoutRef);

        if (layout)
            resources.append(layout);
    }

    const auto createResourceCallback =
        [](nx::vms::common::ResourceDescriptor descriptor) -> QnResourcePtr
        {
            const QString cloudSystemId = crossSystemResourceSystemId(descriptor);
            auto context = appContext()->cloudCrossSystemManager()->systemContext(cloudSystemId);

            if (context)
                return context->createThumbCameraResource(descriptor.id, descriptor.name);

            return QnResourcePtr();
        };

    const MimeData mimeData(d->delayedDrops.raw, createResourceCallback);

    nx::vms::api::ShowreelDataList showreels;
    for (const auto& showreel: systemContext()->showreelManager()->showreels(mimeData.entities()))
        showreels.push_back(showreel);

    resources.append(mimeData.resources());
    if (!resources.isEmpty())
        resourcePool()->addNewResources(resources);

    resources.append(resourcePool()->getResourcesByIds(d->delayedDrops.resourceIds));
    const auto timeStampMs = d->delayedDrops.timeStampMs;

    d->delayedDrops = {};

    if (resources.empty() && showreels.empty())
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

    for (const auto& showreel: showreels)
        menu()->trigger(ReviewShowreelAction, {Qn::UuidRole, showreel.id});
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
        const MimeData mimeData(raw);
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
        d->delayedDrops.resourceIds = startupParameters.customUri.resourceIds;
        d->delayedDrops.timeStampMs = startupParameters.customUri.timestamp;
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

    auto engine = qnClientCoreModule->mainQmlEngine();
    Director::instance()->setupJSEngine(engine);
    testkit::TestKit::setup(engine);

    if (!startupParameters.scriptFile.isEmpty())
        handleScriptFile(startupParameters.scriptFile);

    const auto traceFile = startupParameters.traceFile.isEmpty()
        ? qgetenv("NX_TRACE_FILE").toStdString()
        : startupParameters.traceFile.toStdString();

    if (!traceFile.empty())
    {
        nx::utils::trace::Log::enable(traceFile);
        appContext()->performanceMonitor()->setEnabled(true);
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
    const auto maxTime = qnSyncTime->value();

    milliseconds windowStart(timeStampMs);
    windowStart -= kAcsModeTimelineWindowSize / 2;

    // Adjust period if start time was not passed or passed too near to live.
    if (windowStart.count() < 0 || windowStart + kAcsModeTimelineWindowSize > maxTime)
        windowStart = maxTime - kAcsModeTimelineWindowSize;

    LayoutResourcePtr layout(new LayoutResource());
    layout->setIdUnsafe(QnUuid::createUuid());
    layout->setCellSpacing(0);
    layout->addFlags(Qn::local);
    if (const auto user = context()->user())
        layout->setParentId(user->getId());

    resourcePool()->addResource(layout);

    const auto wlayout = workbench()->addLayout(layout);
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

    if (uri.clientCommand != SystemUri::ClientCommand::Client)
        return false;

    const QString systemAddress = uri.systemAddress;
    if (systemAddress.isEmpty())
        return false;

    const auto systemUrl = nx::utils::Url::fromUserInput(systemAddress);
    nx::network::SocketAddress address(systemUrl.host(), systemUrl.port(0));
    NX_DEBUG(this, "Custom URI: Connecting to the system %1", address);

    LogonData logonData(core::LogonData{.address = address, .credentials = uri.credentials});
    logonData.storeSession = false;
    logonData.secondaryInstance = true;
    logonData.connectScenario = ConnectScenario::connectUsingCommand;

    if (uri.userAuthType == nx::vms::utils::SystemUri::UserAuthType::cloud)
        logonData.userType = nx::vms::api::UserType::cloud;

    if (logonData.userType == nx::vms::api::UserType::cloud
        && logonData.credentials.authToken.empty())
    {
        d->setDelayedLogon(std::make_unique<LogonData>(std::move(logonData)));
    }
    else
    {
        menu()->trigger(ConnectAction, Parameters().withArgument(Qn::LogonDataRole, logonData));
    }
    return true;
}

bool StartupActionsHandler::connectUsingCommandLineAuth(
    const QnStartupParameters& startupParameters)
{
    // Set authentication parameters from the command line.
    LogonData logonData(startupParameters.parseAuthenticationString());
    logonData.connectScenario = ConnectScenario::connectUsingCommand;
    if (logonData.address.isNull())
        return false;

    // Fix compatibility mode command line from old versions.
    const bool isCloudSystem = nx::network::SocketGlobals::addressResolver().isCloudHostname(
        logonData.address.toString());
    if (isCloudSystem && logonData.credentials.authToken.isPassword())
        logonData.userType = nx::vms::api::UserType::cloud;

    NX_DEBUG(this, "Connecting to %1 using command line auth", logonData.address);

    logonData.storeSession = false;
    logonData.secondaryInstance = true;

    if (logonData.userType == nx::vms::api::UserType::cloud)
    {
        d->setDelayedLogon(std::make_unique<LogonData>(std::move(logonData)));
        return true;
    }

    if (!startupParameters.videoWallGuid.isNull())
    {
        logonData.credentials.authToken =
            nx::network::http::VideoWallAuthToken(startupParameters.videoWallGuid);
    }

    menu()->trigger(ConnectAction, Parameters().withArgument(Qn::LogonDataRole, logonData));
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
    return !startupParameters.skipAutoLogin && attemptAutoLogin();
}

bool StartupActionsHandler::attemptAutoLogin()
{
    if (!appContext()->localSettings()->autoLogin()
        || !appContext()->localSettings()->saveCredentialsAllowed())
    {
        return false;
    }

    const auto [url, systemId] = appContext()->localSettings()->lastUsedConnection();

    if (!url.isValid() || systemId.isNull())
        return false;

    auto storedCredentials = nx::vms::client::core::CredentialsManager::credentials(
        systemId,
        url.userName().toStdString());

    LogonData logonData;
    logonData.connectScenario = ConnectScenario::autoConnect;

    if (storedCredentials)
    {
        logonData.address = nx::network::SocketAddress::fromUrl(url);
        logonData.credentials = std::move(*storedCredentials);

        NX_DEBUG(this, "Auto-login to the server %1", logonData.address);
        menu()->trigger(ConnectAction, Parameters().withArgument(Qn::LogonDataRole, logonData));

        return true;
    }
    else if (qnCloudStatusWatcher->status() != core::CloudStatusWatcher::LoggedOut)
    {
        logonData.userType = nx::vms::api::UserType::cloud;
        logonData.address = nx::network::SocketAddress(systemId.toSimpleStdString());

        NX_DEBUG(this, "Auto-login to the Cloud system %1", logonData.address);
        d->setDelayedLogon(std::make_unique<LogonData>(std::move(logonData)));

        return true;
    }

    return false;
}

bool StartupActionsHandler::connectToCloudIfNeeded(const QnStartupParameters& startupParameters)
{
    core::CloudAuthData authData;

    const auto& uri = startupParameters.customUri;
    if (uri.isValid()
        && (uri.hasCloudSystemAddress()
            || uri.clientCommand == nx::vms::utils::SystemUri::ClientCommand::LoginToCloud))
    {
        authData.authorizationCode = uri.authCode.toStdString();
        NX_DEBUG(
            this, "Received authorization code, length: %1", authData.authorizationCode.size());
    }

    if (authData.empty())
    {
        authData = appContext()->coreSettings()->cloudAuthData();
        NX_DEBUG(
            this, "Loaded auth data, refresh token length: %1", authData.refreshToken.length());
    }

    if (authData.empty())
        return false;

    NX_DEBUG(this, "Connecting to cloud as %1", authData.credentials.username);
    appContext()->cloudStatusWatcher()->setInitialAuthData(authData);
    return true;
}

} // namespace nx::vms::client::desktop
