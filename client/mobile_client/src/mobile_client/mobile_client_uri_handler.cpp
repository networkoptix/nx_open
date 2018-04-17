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
using nx::client::core::OperationManager;

class QnMobileClientUriHandler::Private: public QObject
{
public:
    Private(QnContext* context);

    void loginToCloud(
        const SystemUri::Auth& auth,
        bool forceLoginScreen = false,
        const QString& connectOperationId = QString());

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
        const QString& connectOperationId);

    void connectedCallback(const SystemUri& uri);

    using SuccessConnectionCallback = std::function<void ()>;
    void showConnectToServerByHostScreen(const SystemUri& uri,
        const SuccessConnectionCallback& successConnectionCallback);

private:
    QString directCloudConnectionOperationId;
    QString connectToServerOperationId;
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
        [this](Qn::ConnectionResult /*status*/, const QVariant &/*infoParameter*/)
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
    if (directCloudConnectionOperationId.isEmpty())
        return;

    const bool success = cloudStatusWatcher->status() == QnCloudStatusWatcher::Online;
    const auto tmpId = directCloudConnectionOperationId;
    directCloudConnectionOperationId.clear();
    operationManager->finishOperation(tmpId, success);
}

void QnMobileClientUriHandler::Private::handleConnectionStateChanged(bool connected)
{
    if (connectToServerOperationId.isEmpty())
        return;

    const auto tmpId = connectToServerOperationId;
    connectToServerOperationId.clear();
    operationManager->finishOperation(tmpId, connected);
}

void QnMobileClientUriHandler::Private::loginToCloudDirectlyInternal(
    const SystemUri::Auth& auth,
    const QString& connectOperationId)
{
    NX_EXPECT(directCloudConnectionOperationId.isEmpty(),
        "Double connect-to-cloud operation requested");

    uiController->disconnectFromSystem();

    const auto callback =
        [this, auth, connectOperationId](bool success)
        {
            if (!success)
                loginToCloud(auth, true, connectOperationId);
        };

    const auto credentials = QnEncodedCredentials(auth.user, auth.password);
    cloudStatusWatcher->setCredentials(credentials);
    directCloudConnectionOperationId = operationManager->startOperation(callback);
}

void QnMobileClientUriHandler::Private::loginToCloud(
    const SystemUri::Auth& auth,
    bool forceLoginScreen,
    const QString& connectOperationId)
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
        operationManager->finishOperation(connectOperationId, true);
        return;
    }

    if (!user.isEmpty() && !password.isEmpty() && !forceLoginScreen)
    {
        loginToCloudDirectlyInternal(auth, connectOperationId);
        return;
    }

    if (user.isEmpty())
    {
        // Shows login to cloud screen only if user and passowrd are both empty
        // and we logged out from cloud. Otherwise we just accept previously made connection.
        if (!password.isEmpty())
        {
            NX_EXPECT(false, "We dont support password without user");
        }
        else if (cloudStatusWatcher->status() == QnCloudStatusWatcher::LoggedOut)
        {
            uiController->disconnectFromSystem();
            uiController->openLoginToCloudScreen(user, password, connectOperationId);
        }
        return;
    }

    // We don't reset current cloud credentials to give user opportunity to cancel requested
    // login to cloud action.
    uiController->disconnectFromSystem();
    uiController->openLoginToCloudScreen(user, password, connectOperationId);
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

    const auto operationId = operationManager->startOperation(callback);
    uiController->openConnectToServerScreen(uri.connectionUrl(), operationId);
}

void QnMobileClientUriHandler::Private::connectToServerDirectly(const SystemUri& uri)
{
    if (!uiController)
        return;

    NX_EXPECT(connectToServerOperationId.isEmpty(),
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

        connectToServerOperationId = operationManager->startOperation(callback);
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
        const auto operationId = operationManager->startOperation(connectToServerCallback);
        loginToCloud(uri.authenticator(), false, operationId);
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
