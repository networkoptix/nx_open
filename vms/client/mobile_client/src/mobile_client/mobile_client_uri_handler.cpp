#include "mobile_client_uri_handler.h"

#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/vms/utils/system_uri.h>
#include <watchers/cloud_status_watcher.h>
#include <nx/client/core/utils/operation_manager.h>

#include <common/common_module.h>
#include <context/context.h>
#include <context/connection_manager.h>
#include <client_core/client_core_module.h>

#include "mobile_client_ui_controller.h"
#include "mobile_client_settings.h"

using nx::vms::utils::SystemUri;
using nx::vms::client::core::OperationManager;

class QnMobileClientUriHandler::Private: public QObject
{
public:
    Private(QnContext* context);

    void loginToCloud(
        const SystemUri::Auth& auth,
        bool forceLoginScreen = false,
        const QString& connectHandle = QString());

    void handleClientCommand(const SystemUri& uri);

public:
    const QPointer<OperationManager> operationManager;
    const QPointer<QnMobileClientUiController> uiController;
    const QPointer<QnConnectionManager> connectionManager;
    const QPointer<QnCloudStatusWatcher> cloudStatusWatcher;

private:
    void connectToServerDirectly(const SystemUri& uri);

    void handleCloudStatusChanged();
    void handleConnectionStateChanged(bool connected);

    void loginToCloudDirectlyInternal(
        const SystemUri::Auth& aguth,
        const QString& connectHandle);

    void connectedCallback(const SystemUri& uri);

    using SuccessConnectionCallback = std::function<void ()>;
    void showConnectToServerByHostScreen(const SystemUri& uri,
        const SuccessConnectionCallback& successConnectionCallback);

private:
    QString directCloudConnectionHandle;
    QString connectToServerHandle;
};

QnMobileClientUriHandler::Private::Private(QnContext* context):
    operationManager(context->operationManager()),
    uiController(context->uiController()),
    connectionManager(context->connectionManager()),
    cloudStatusWatcher(context->cloudStatusWatcher())
{
    connect(cloudStatusWatcher, &QnCloudStatusWatcher::statusChanged,
        this, &Private::handleCloudStatusChanged);
    connect(cloudStatusWatcher, &QnCloudStatusWatcher::errorChanged,
        this, &Private::handleCloudStatusChanged);

    const auto connectionManager = context->connectionManager();
    connect(connectionManager, &QnConnectionManager::connectionFailed, this,
        [this](Qn::ConnectionResult /*status*/, const QVariant& /*infoParameter*/)
        {
            handleConnectionStateChanged(false);
        });
    connect(connectionManager, &QnConnectionManager::connected, this,
        [this](bool initialConnect)
        {
            if (initialConnect)
                handleConnectionStateChanged(true);
        });
}

void QnMobileClientUriHandler::Private::handleCloudStatusChanged()
{
    if (directCloudConnectionHandle.isEmpty())
        return;

    const bool success = cloudStatusWatcher->status() == QnCloudStatusWatcher::Online;
    const auto tmpHandle = directCloudConnectionHandle;
    directCloudConnectionHandle.clear();
    operationManager->finishOperation(tmpHandle, success);
}

void QnMobileClientUriHandler::Private::handleConnectionStateChanged(bool connected)
{
    if (connectToServerHandle.isEmpty())
        return;

    const auto tmpHandle = connectToServerHandle;
    connectToServerHandle.clear();
    operationManager->finishOperation(tmpHandle, connected);
}

void QnMobileClientUriHandler::Private::loginToCloudDirectlyInternal(
    const SystemUri::Auth& auth,
    const QString& connectHandle)
{
    NX_ASSERT(directCloudConnectionHandle.isEmpty(),
        "Double connect-to-cloud operation requested");

    uiController->disconnectFromSystem();

    const auto callback =
        [this, auth, connectHandle](bool success)
        {
            if (!success)
                loginToCloud(auth, true, connectHandle);
        };

    const nx::vms::common::Credentials credentials(auth.user, auth.password);
    cloudStatusWatcher->setCredentials(credentials);
    directCloudConnectionHandle = operationManager->startOperation(callback);
}

void QnMobileClientUriHandler::Private::loginToCloud(
    const SystemUri::Auth& auth,
    bool forceLoginScreen,
    const QString& connectHandle)
{
    if (!uiController)
        return;

    const auto user = auth.user;
    const auto password = auth.password;
    const bool loggedInWithSameCredentials =
        !user.isEmpty()
        && !password.isEmpty()
        && user == cloudStatusWatcher->cloudLogin()
        && password == cloudStatusWatcher->cloudPassword()
        && cloudStatusWatcher->status() == QnCloudStatusWatcher::Online;

    if (loggedInWithSameCredentials)
    {
        operationManager->finishOperation(connectHandle, true);
        return;
    }

    if (!user.isEmpty() && !password.isEmpty() && !forceLoginScreen)
    {
        loginToCloudDirectlyInternal(auth, connectHandle);
        return;
    }

    if (user.isEmpty())
    {
        // Shows login to cloud screen only if user and passowrd are both empty
        // and we logged out from cloud. Otherwise we just accept previously made connection.
        if (!password.isEmpty())
        {
            NX_ASSERT(false, "We dont support password without user");
        }
        else if (cloudStatusWatcher->status() == QnCloudStatusWatcher::LoggedOut)
        {
            uiController->disconnectFromSystem();
            uiController->openLoginToCloudScreen(user, password, connectHandle);
        }
        return;
    }

    // We don't reset current cloud credentials to give user opportunity to cancel requested
    // login to cloud action.
    uiController->disconnectFromSystem();
    uiController->openLoginToCloudScreen(user, password, connectHandle);
}

void QnMobileClientUriHandler::Private::connectedCallback(const SystemUri& uri)
{
    if (!uiController)
        return;

    const auto resourceIds = uri.resourceIds();
    if (resourceIds.isEmpty() || uri.systemAction() != SystemUri::SystemAction::View)
        return;

    const auto timestamp = uri.timestamp();
    if (timestamp != -1)
        uiController->openVideoScreen(resourceIds.first(), timestamp);
    else
        uiController->openResourcesScreen(resourceIds);
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
    uiController->openConnectToServerScreen(uri.connectionUrl(), handle);
}

void QnMobileClientUriHandler::Private::connectToServerDirectly(const SystemUri& uri)
{
    if (!uiController)
        return;

    NX_ASSERT(connectToServerHandle.isEmpty(),
        "Double connect-to-server operation requested.");

    auto url = uri.connectionUrl();
    uiController->disconnectFromSystem();
    if (!url.isValid())
        return;

    const auto auth = uri.authenticator();
    if (auth.password.isEmpty() || auth.user.isEmpty())
    {
        showConnectToServerByHostScreen(uri, [this, uri]() { connectedCallback(uri); });
        return;
    }

    if (uri.hasCloudSystemId())
    {
        url.setUserName(cloudStatusWatcher->cloudLogin());
        url.setPassword(cloudStatusWatcher->cloudPassword());
    }

    const auto resourceIds = uri.resourceIds();
    if (!resourceIds.isEmpty() && uri.systemAction() == SystemUri::SystemAction::View)
    {
        const auto callback =
            [this, uri](bool success)
            {
                if (success)
                    connectedCallback(uri);
                else
                    showConnectToServerByHostScreen(uri, [this, uri]() { connectedCallback(uri); });
            };

        connectToServerHandle = operationManager->startOperation(callback);
    }

    uiController->connectToSystem(url);
}

void QnMobileClientUriHandler::Private::handleClientCommand(const SystemUri& uri)
{
    if (!uiController || uri.clientCommand() != SystemUri::ClientCommand::Client)
        return;

    if (uri.systemId().isEmpty())
        return;

    const auto auth = uri.authenticator();
    if (uri.hasCloudSystemId())
    {
        const auto connectToServerCallback =
            [this, uri](bool success)
            {
                if (success)
                    connectToServerDirectly(uri);
            };
        const auto Handle = operationManager->startOperation(connectToServerCallback);
        loginToCloud(uri.authenticator(), false, Handle);
    }
    else
    {
        connectToServerDirectly(uri);
    }
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

    return result;
}

const char*QnMobileClientUriHandler::handlerMethodName()
{
    return "handleUrl";
}

void QnMobileClientUriHandler::handleUrl(const QUrl& nativeUrl)
{
    const auto url = nx::utils::Url::fromQUrl(nativeUrl);
    SystemUri uri(url);

    if (!uri.isValid())
    {
        // Open external URLs.
        if (url.isValid())
            QDesktopServices::openUrl(url.toQUrl());
        return;
    }

    if (uri.referral().source == SystemUri::ReferralSource::MobileClient)
    {
        // Ignore our own URL requests.
        QDesktopServices::openUrl(url.toQUrl());
        return;
    }

    if (uri.protocol() != SystemUri::Protocol::Native
        && uri.domain() != nx::network::SocketGlobals::cloud().cloudHost())
    {
        if (uri.scope() == SystemUri::Scope::Generic)
        {
            // Re-call openUrl to let QDesktopServices open URL in browser.
            QDesktopServices::openUrl(url.toQUrl());
        }

        return;
    }

    if (!d->uiController)
        return;

    switch (uri.clientCommand())
    {
        case SystemUri::ClientCommand::None:
            break;
        case SystemUri::ClientCommand::Client:
            d->handleClientCommand(uri);
            break;
        case SystemUri::ClientCommand::LoginToCloud:
            d->loginToCloud(uri.authenticator());
            break;
        case SystemUri::ClientCommand::OpenOnPortal:
            break;
    }
}
