// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_context.h"

#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickWindow>

#include <context/context.h>
#include <mobile_client/mobile_client_uri_handler.h>
#include <nx/build_info.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/application_context.h>
#include <ui/texture_size_helper.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::mobile {

struct WindowContext::Private
{
    const nx::Uuid peerId = nx::Uuid::createUuid();
    const std::unique_ptr<SessionManager> sessionManager;
    std::unique_ptr<SystemContext> systemContext;
    std::unique_ptr<QnContext> uiContext;

    // TODO: #ynikitenkov Move to the application contex.
    std::unique_ptr<QnMobileClientUriHandler> uriHandler;

    QPointer<QQuickWindow> mainWindow;
    std::unique_ptr<QnTextureSizeHelper> textureSizeHelper;
};

//-------------------------------------------------------------------------------------------------

WindowContext::WindowContext(QObject* parent):
    base_type(parent),
    d{new Private{.sessionManager = std::make_unique<SessionManager>(this)}}
{
    nx::network::SocketGlobals::cloud().outgoingTunnelPool().assignOwnPeerId("mc", d->peerId);

    d->uiContext = std::make_unique<QnContext>(d->sessionManager.get());
    d->uriHandler = std::make_unique<QnMobileClientUriHandler>(d->uiContext.get());

    const auto engine = appContext()->qmlEngine();
    engine->rootContext()->setContextObject(d->uiContext.get());
    engine->setObjectOwnership(d->uiContext.get(), QQmlEngine::CppOwnership);
}

WindowContext::~WindowContext()
{
    d->sessionManager->resetSession();
}

void WindowContext::initializeWindow()
{
    QQmlComponent mainComponent(appContext()->qmlEngine(), QUrl("main.qml"));
    d->mainWindow = qobject_cast<QQuickWindow*>(mainComponent.create());
    d->textureSizeHelper = std::make_unique<QnTextureSizeHelper>(d->mainWindow);

    if (!mainComponent.errors().isEmpty())
        return;

    if (!nx::build_info::isMobile())
    {
        d->mainWindow->setWidth(380);
        d->mainWindow->setHeight(650);
    }
}

QQuickWindow* WindowContext::mainWindow() const
{
    return d->mainWindow.get();
}

QnTextureSizeHelper* WindowContext::textureSizeHelper() const
{
    return d->textureSizeHelper.get();
}

SystemContext* WindowContext::mainSystemContext()
{
    return d->systemContext.get();
}

SessionManager* WindowContext::sessionManager() const
{
    return d->sessionManager.get();
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
