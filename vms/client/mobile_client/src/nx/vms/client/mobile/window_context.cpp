// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_context.h"

#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickWindow>

#include <mobile_client/mobile_client_ui_controller.h>
#include <mobile_client/mobile_client_uri_handler.h>
#include <nx/build_info.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/remote_connection_error_strings.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/push_notification/push_notification_manager.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/maintenance/remote_log_manager.h>
#include <nx/vms/client/mobile/ui/ui_controller.h>
#include <ui/texture_size_helper.h>
#include <ui/window_utils.h>

namespace nx::vms::client::mobile {

struct WindowContext::Private
{
    WindowContext* const q;
    const nx::Uuid peerId = nx::Uuid::createUuid();
    const std::unique_ptr<SessionManager> sessionManager;

    std::unique_ptr<SystemContext> systemContext;
    std::unique_ptr<RemoteLogManager> remoteLogManager;
    std::unique_ptr<UiController> uiController;

    // TODO: #ynikitenkov Move to the application contex.
    std::unique_ptr<QnMobileClientUriHandler> uriHandler;

    QPointer<QQuickWindow> mainWindow;

    std::unique_ptr<QnMobileClientUiController> depricatedUiController;

    void initializeDepricatedClasses();
    void initializeMainWindow();
    void initializeCloudStatusHandling();
};

void WindowContext::Private::initializeDepricatedClasses()
{
    depricatedUiController = std::make_unique<QnMobileClientUiController>(q);
}

void WindowContext::Private::initializeMainWindow()
{
    uiController = std::make_unique<UiController>(q);

    const auto engine = appContext()->qmlEngine();
    QQmlComponent mainComponent(engine, QUrl("main.qml"));
    mainWindow = qobject_cast<QQuickWindow*>(mainComponent.create());

    if (!nx::build_info::isMobile() && mainWindow)
    {
        mainWindow->setWidth(380);
        mainWindow->setHeight(650);
    }

    if (!mainComponent.errors().isEmpty())
        qWarning() << mainComponent.errorString();

    prepareWindow();
}

void WindowContext::Private::initializeCloudStatusHandling()
{
    const auto watcher = appContext()->cloudStatusWatcher();
    connect(watcher, &core::CloudStatusWatcher::statusChanged, q,
        [this, watcher]()
        {
            if (watcher->status() != core::CloudStatusWatcher::LoggedOut)
                return;

            const auto settings = appContext()->coreSettings();
            settings->digestCloudPassword = {};

            const auto& data = settings->lastConnection();
            if (data.url.scheme() == Session::kCloudUrlScheme
                || nx::network::SocketGlobals::addressResolver().isCloudHostname(data.url.host()))
            {
                q->sessionManager()->stopSessionByUser();
            }
        });

    connect(watcher, &core::CloudStatusWatcher::forcedLogout, q,
        [this]()
        {
            const auto description =
                core::errorDescription(core::RemoteConnectionErrorCode::cloudSessionExpired, {}, {});
            q->uiController()->showMessage(description.shortText, description.longText);
        });
}

//-------------------------------------------------------------------------------------------------

WindowContext::WindowContext(QObject* parent):
    base_type(parent),
    d{new Private{.q = this, .sessionManager = std::make_unique<SessionManager>(this)}}
{
    nx::network::SocketGlobals::cloud().outgoingTunnelPool().assignOwnPeerId("mc", d->peerId);

    d->remoteLogManager = std::make_unique<RemoteLogManager>();
    d->initializeDepricatedClasses();
    d->initializeMainWindow();
    d->initializeCloudStatusHandling();

    d->uriHandler = std::make_unique<QnMobileClientUriHandler>(d->depricatedUiController.get());

    connect(appContext()->pushManager(), &PushNotificationManager::showPushActivateErrorMessage,
        d->uiController.get(), &UiController::showMessage);
}

WindowContext::~WindowContext()
{
    sessionManager()->setSuspended(true);
    if (const auto context = mainSystemContext())
        context->setSession({});
    d->sessionManager->resetSession();
}

QQuickWindow* WindowContext::mainWindow() const
{
    return d->mainWindow.get();
}

SystemContext* WindowContext::mainSystemContext()
{
    return d->systemContext.get();
}

UiController* WindowContext::uiController() const
{
    return d->uiController.get();
}

QQuickWindow* WindowContext::window() const
{
    return d->mainWindow.get();
}

SessionManager* WindowContext::sessionManager() const
{
    return d->sessionManager.get();
}

RemoteLogManager* WindowContext::logManager() const
{
    return d->remoteLogManager.get();
}

QnMobileClientUiController* WindowContext::depricatedUiController() const
{
    return d->depricatedUiController.get();
}

SystemContext* WindowContext::createSystemContext()
{
    NX_ASSERT(!d->systemContext);
    d->systemContext = std::make_unique<SystemContext>(
        this,
        core::SystemContext::Mode::client,
        d->peerId);
    appContext()->addSystemContext(d->systemContext.get());

    emit mainSystemContextChanged();
    return d->systemContext.get();
}

void WindowContext::deleteSystemContext(SystemContext* context)
{
    d->systemContext->stopLongRunnables();

    appContext()->removeSystemContext(context);
    d->systemContext.reset();
    emit mainSystemContextChanged();
}

} // namespace nx::vms::client::mobile
