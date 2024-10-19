// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mobile_client_uri_handler.h"

#include <QtGui/QDesktopServices>

#include <context/context.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/branding.h>
#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/oauth_client.h>
#include <nx/vms/client/core/network/oauth_client_constants.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/mobile/controllers/web_view_controller.h>
#include <nx/vms/client/mobile/current_system_context_aware.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/ui/qml_wrapper_helper.h>
#include <nx/vms/client/mobile/utils/operation_manager.h>
#include <nx/vms/utils/system_uri.h>
#include <utils/common/delayed.h>

#include "mobile_client_module.h"
#include "mobile_client_settings.h"
#include "mobile_client_ui_controller.h"

using nx::vms::utils::SystemUri;
using nx::vms::client::core::CloudStatusWatcher;
using nx::vms::client::mobile::WebViewController;

namespace {

nx::utils::Url connectionUrl(const SystemUri& uri)
{
    if (uri.systemAddress.isEmpty())
        return nx::utils::Url();

    nx::utils::Url result;
    if (uri.protocol == SystemUri::Protocol::Http)
        result.setScheme(nx::network::http::kUrlSchemeName);
    else
        result.setScheme(nx::network::http::kSecureUrlSchemeName);

    QUrl address = QUrl::fromUserInput(uri.systemAddress);
    result.setHost(address.host());
    result.setPort(address.port());
    if (uri.credentials.authToken.isPassword())
    {
        result.setUserName(uri.credentials.username);
        result.setPassword(uri.credentials.authToken.value);
    }

    return result;
}

} // namespace

class QnMobileClientUriHandler::Private: public QObject,
    public nx::vms::client::mobile::CurrentSystemContextAware
{
public:
    Private(QnContext* context);

    bool waitForCloudLogIn();
    bool tryConnectToCloudWithCode(const QString& authCode);
    bool loginToCloud(const SystemUri& uri);

    void handleClientCommand(const SystemUri& uri);

public:
    const QPointer<nx::vms::client::mobile::OperationManager> operationManager;
    const QPointer<QnMobileClientUiController> uiController;
    const QPointer<CloudStatusWatcher> cloudStatusWatcher;
    const QPointer<nx::vms::client::mobile::SessionManager> sessionManager;
    const QPointer<nx::vms::client::mobile::WebViewController> webViewController;

private:
    void connectToServerDirectly(const SystemUri& uri);

    void handleCloudStatusChanged();

    void connectedCallback(const SystemUri& uri);

    using SuccessConnectionCallback = std::function<void ()>;
    void showConnectToServerByHostScreen(const SystemUri& uri,
        const SuccessConnectionCallback& successConnectionCallback);

    void switchToSessionsScreen();
};

QnMobileClientUriHandler::Private::Private(QnContext* context):
    operationManager(context->operationManager()),
    uiController(context->uiController()),
    cloudStatusWatcher(context->cloudStatusWatcher()),
    sessionManager(context->sessionManager()),
    webViewController(qnMobileClientModule->webViewController())
{
}

bool QnMobileClientUriHandler::Private::waitForCloudLogIn()
{
    bool result = false;
    QEventLoop loop;
    const auto handleCloudStatusChanged =
        [&result, &loop](CloudStatusWatcher::Status status)
    {
        switch (status)
        {
            case CloudStatusWatcher::Online:
                result = true;
                [[fallthrough]];
            case CloudStatusWatcher::LoggedOut:
                loop.quit();
                break;
            default:
                break;
        };
    };

    const auto handleErrorChanged =
        [&loop](CloudStatusWatcher::ErrorCode error)
    {
        if (error != CloudStatusWatcher::NoError)
            loop.quit();
    };

    connect(cloudStatusWatcher, &CloudStatusWatcher::statusChanged,
        this, handleCloudStatusChanged);
    connect(cloudStatusWatcher, &CloudStatusWatcher::errorChanged,
        this, handleErrorChanged);

    loop.exec();
    cloudStatusWatcher->disconnect(this);
    return result;
}

bool QnMobileClientUriHandler::Private::tryConnectToCloudWithCode(const QString& authCode)
{
    if (authCode.isEmpty())
        return false;

    using namespace nx::vms::client::core;
    OauthClient oauth(
        OauthClientType::loginCloud, OauthViewType::mobile);

    QEventLoop loop;
    bool hasAuthDataReady = false;
    connect(&oauth, &OauthClient::authDataReady, this,
        [this, &loop, &oauth, &hasAuthDataReady]()
        {
            hasAuthDataReady = true;
            cloudStatusWatcher->setAuthData(oauth.authData(), CloudStatusWatcher::AuthMode::login);
            loop.quit();
        });
    connect(&oauth, &OauthClient::authDataReady, &loop, &QEventLoop::quit);
    oauth.setCode(authCode);
    loop.exec();

    return hasAuthDataReady && waitForCloudLogIn();
}

bool QnMobileClientUriHandler::Private::loginToCloud(const SystemUri& uri)
{
    NX_DEBUG(this, "loginToCloud(): start");
    if (!NX_ASSERT(uiController, "UI controller is not ready"))
        return false;

    const bool loggedInWithSameCredentials =
        uri.credentials.username == cloudStatusWatcher->cloudLogin().toStdString()
        && cloudStatusWatcher->status() != CloudStatusWatcher::LoggedOut;

    if (loggedInWithSameCredentials)
    {
        NX_DEBUG(this, "loginToCloud(): end: logged in with the same credentials");
        return true;
    }

    if (tryConnectToCloudWithCode(uri.authCode))
    {
        NX_DEBUG(this, "loginToCloud(): end: logged in with the specified access code");
        return true;
    }

    if (uri.credentials.username.empty()
        && cloudStatusWatcher->status() != CloudStatusWatcher::LoggedOut)
    {
        NX_DEBUG(this,
            "loginToCloud(): user is empty, trying to use current cloud connection (if any)");
        return true;
    }
    cloudStatusWatcher->resetAuthData();
    sessionManager->stopSession();

    QVariantMap properties;
    properties["user"] = QString::fromStdString(uri.credentials.username);
    const bool authorized = nx::vms::client::mobile::QmlWrapperHelper::showScreen(
        QUrl("qrc:qml/Nx/Screens/Cloud/Login.qml"), properties) == "success";

    NX_DEBUG(this, "loginToCloud(): authorized on cloud: %1", authorized);

    return authorized && waitForCloudLogIn();
}

void QnMobileClientUriHandler::Private::connectedCallback(const SystemUri& uri)
{
    const auto resourceIds = uri.resourceIds;
    NX_DEBUG(this, "connectedCallback(): start: resources count is <%1>", resourceIds);
    if (!NX_ASSERT(uiController, "UI controller is not ready"))
        return;

    if (resourceIds.isEmpty() || uri.systemAction != SystemUri::SystemAction::View)
    {
        NX_DEBUG(this, "connectedCallback(): end: nothing to open or action is not 'View'");
        return;
    }

    const auto timestamp = uri.timestamp;
    if (timestamp != -1 || resourceIds.size() == 1)
    {
        NX_DEBUG(this,
            "connectedCallback(): end: opening video screen for resource %1 with timestamp <%2>",
            resourceIds.first().toString(), timestamp);

        const auto camera = systemContext()->resourcePool()->getResourceById(resourceIds.first());
        uiController->openVideoScreen(nx::vms::client::core::withCppOwnership(camera), timestamp);
    }
    else
    {
        NX_DEBUG(this, "connectedCallback(): end: opening resources screen");
        uiController->openResourcesScreen(resourceIds);
    }
}

void QnMobileClientUriHandler::Private::showConnectToServerByHostScreen(const SystemUri& uri,
    const SuccessConnectionCallback& successConnectionCallback)
{
    if (!uiController)
        return;

    const auto callback =
        [successConnectionCallback](bool success)
        {
            if (success)
                successConnectionCallback();
        };

    const auto handle = operationManager->startOperation(callback);
    uiController->openConnectToServerScreen(connectionUrl(uri), handle);
}

void QnMobileClientUriHandler::Private::switchToSessionsScreen()
{
    uiController->sessionsScreenRequested();
    using Screen = QnMobileClientUiController::Screen;
    if (uiController->currentScreen() == Screen::SessionsScreen)
        return;

    QEventLoop loop;
    const auto screenChangeHandler =
        [this, &loop]()
        {
            if (uiController->currentScreen() == Screen::SessionsScreen)
                loop.quit();
        };

    const auto connection =
        connect(uiController, &QnMobileClientUiController::currentScreenChanged,
            this, screenChangeHandler);
    loop.exec();

    QObject::disconnect(connection);
}

void QnMobileClientUriHandler::Private::connectToServerDirectly(const SystemUri& uri)
{
    NX_DEBUG(this, "connectToServerDirectly(): start");
    if (!NX_ASSERT(sessionManager, "Session manager is not ready"))
        return;

    sessionManager->stopSession();

    // Switch to sessions screen and wait for the change to avoid side effects with a login process.
    switchToSessionsScreen();

    auto url = connectionUrl(uri);
    if (!url.isValid())
    {
        NX_DEBUG(this, "connectToServerDirectly(): end: url is not valid");
        return;
    }

    const bool hasCloudSystem = uri.hasCloudSystemAddress();
    if (!hasCloudSystem && (uri.credentials.authToken.empty() || uri.credentials.username.empty()))
    {
        NX_DEBUG(this, "connectToServerDirectly(): end: showing connect to server screen");
        showConnectToServerByHostScreen(uri, [this, uri]() { connectedCallback(uri); });
        return;
    }

    using RemoteConnectionErrorCode = nx::vms::client::core::RemoteConnectionErrorCode;
    const auto callback =
        [this, hasCloudSystem, uri](
            std::optional<RemoteConnectionErrorCode> errorCode)
        {
            const bool success = !errorCode;
            NX_DEBUG(this, "connectToServerDirectly(): callback: successfull is <%1>", success);
            if (success)
                connectedCallback(uri);
            else if (!hasCloudSystem)
                showConnectToServerByHostScreen(uri, [this, uri]() { connectedCallback(uri); });
        };

    if (hasCloudSystem)
    {
        NX_DEBUG(this, "connectToServerDirectly(): end: starting cloud session");

        using namespace nx::network::http;
        const auto digestCredentials =
            [uri]() -> std::optional<Credentials>
            {
                if (uri.credentials.authToken.empty() || uri.credentials.username.empty())
                    return {};

                return uri.credentials;
            }();

        sessionManager->startCloudSession(uri.systemAddress, digestCredentials, callback);
    }
    else
    {
        NX_DEBUG(this, "connectToServerDirectly(): end: starting local session");
        sessionManager->startSessionByUrl(url, callback);
    }
}

void QnMobileClientUriHandler::Private::handleClientCommand(const SystemUri& uri)
{
    NX_DEBUG(this, "handleClientCommand(): start: url");

    if (!NX_ASSERT(uiController, "UI controller is not ready"))
        return;

    if (!NX_ASSERT(uri.clientCommand == SystemUri::ClientCommand::Client))
        return;

    if (uri.systemAddress.isEmpty())
    {
        NX_DEBUG(this, "handleClientCommand(): end: system id is empty");
        return;
    }

    if (uri.hasCloudSystemAddress())
    {
        NX_DEBUG(this,
            "handleClientCommand(): uri contains cloud system id, logging in to the cloud");

        if (!loginToCloud(uri))
            return;
    }

    NX_DEBUG(this, "handleClientCommand(): connecting to the server directly");
    connectToServerDirectly(uri);

    NX_DEBUG(this, "handleClientCommand(): end");
}

//-------------------------------------------------------------------------------------------------

QnMobileClientUriHandler::QnMobileClientUriHandler(QnContext* context, QObject* parent):
    base_type(parent),
    d(new Private(context))
{
}

QnMobileClientUriHandler::~QnMobileClientUriHandler()
{
}

QStringList QnMobileClientUriHandler::supportedSchemes()
{
    const auto protocols = {
        SystemUri::Protocol::Native,
        SystemUri::Protocol::Http,
        SystemUri::Protocol::Https};

    QStringList result;
    for (auto protocol: protocols)
        result.append(SystemUri::toString(protocol));

    result.append(nx::branding::compatibleUriProtocols());

    return result;
}

const char*QnMobileClientUriHandler::handlerMethodName()
{
    return "handleUrl";
}

void QnMobileClientUriHandler::handleUrl(const QUrl& nativeUrl)
{
    NX_DEBUG(this, "handleUrl(): start: url is <%1>", nativeUrl.toString(QUrl::RemovePassword));

    const auto url = nx::utils::Url::fromQUrl(nativeUrl);
    SystemUri uri(url);

    if (!uri.isValid(/*requireAuthForCloudSystemConnection*/ false))
    {
        NX_DEBUG(this, "handleUrl(): end: opening external url by default services");
        if (url.isValid())
            QDesktopServices::openUrl(url.toQUrl());

        return;
    }

    if (uri.referral.source == SystemUri::ReferralSource::MobileClient)
    {
        NX_DEBUG(this, "handleUrl(): end: ignoring our own URL requests");
        QDesktopServices::openUrl(url.toQUrl());
        return;
    }

    if (uri.protocol != SystemUri::Protocol::Native
        && uri.cloudHost.toStdString() != nx::network::SocketGlobals::cloud().cloudHost())
    {
        if (uri.scope == SystemUri::Scope::generic)
        {
            NX_DEBUG(this,
                "handleUrl(): end: re-call openUrl to let Qt services open URL in browser");
            QDesktopServices::openUrl(url.toQUrl());
        }
        else
        {
            NX_DEBUG(this, "handleUrl(): invalid protocol or uri domain");
        }

        return;
    }

    if (!d->uiController)
    {
        NX_ASSERT(false, "handleUrl(): end: UI controller is not ready, should not get here");
        return;
    }

    d->webViewController->closeWindow();

    switch (uri.clientCommand)
    {
        case SystemUri::ClientCommand::None:
            break;
        case SystemUri::ClientCommand::Client:
            NX_DEBUG(this, "handleUrl(): trying to handle client command");
            executeLaterInThread(
                [this, uri]() { d->handleClientCommand(uri); },
                qApp->thread());
            break;
        case SystemUri::ClientCommand::LoginToCloud:
            NX_DEBUG(this, "handleUrl(): trying to log in to the cloud");
            executeLaterInThread(
                [this, uri]() { d->loginToCloud(uri); },
                qApp->thread());
            break;
        case SystemUri::ClientCommand::OpenOnPortal:
            break;
    }

    NX_DEBUG(this, "handleUrl(): end");
}
