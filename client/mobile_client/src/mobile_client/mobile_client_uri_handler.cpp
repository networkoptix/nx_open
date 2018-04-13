#include "mobile_client_uri_handler.h"

#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/vms/utils/system_uri.h>
#include <watchers/cloud_status_watcher.h>
#include <nx/client/core/utils/operation_manager.h>

#include <common/common_module.h>
#include <context/context.h>
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
    const QPointer<QnCloudStatusWatcher> cloudStatusWatcher;

private:
    void handleClientCommandConnectionPrepared(
        const SystemUri& uri,
        bool success);

    void handleCloudStatusChanged();

    void loginToCloudDirectlyInternal(
        const SystemUri::Auth& aguth,
        const QString& connectOperationId);

private:
    QString directCloudConnectionOperationId;
};

QnMobileClientUriHandler::Private::Private(QnContext* context):
    operationManager(context->operationManager()),
    uiController(context->uiController()),
    cloudStatusWatcher(context->cloudStatusWatcher())
{
    connect(cloudStatusWatcher, &QnCloudStatusWatcher::statusChanged,
        this, &Private::handleCloudStatusChanged);
    connect(cloudStatusWatcher, &QnCloudStatusWatcher::errorChanged,
        this, &Private::handleCloudStatusChanged);
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

void QnMobileClientUriHandler::Private::handleClientCommandConnectionPrepared(
    const SystemUri& uri,
    bool success)
{
    if (!uiController)
        return;

    auto url = uri.connectionUrl();
    uiController->disconnectFromSystem();
    if (!success || !url.isValid())
        return;

    if (uri.hasCloudSystemId())
    {
        url.setUserName(cloudStatusWatcher->cloudLogin());
        url.setPassword(cloudStatusWatcher->cloudPassword());
    }

    const auto resourceIds = uri.systemAction() == SystemUri::SystemAction::View
        ? uri.resourceIds()
        : SystemUri::ResourceIdList();

    uiController->connectToSystem(url, resourceIds);
}

void QnMobileClientUriHandler::Private::handleClientCommand(const SystemUri& uri)
{
    if (!uiController || uri.clientCommand() != SystemUri::ClientCommand::Client)
        return;

    if (uri.systemId().isEmpty())
        return;

    const auto auth = uri.authenticator();
    const bool cloudConnection = uri.hasCloudSystemId();
    const auto connectToSystemCallback =
        [this, uri](bool success) { handleClientCommandConnectionPrepared(uri, success); };

    if (cloudConnection)
    {
        const auto operationId = operationManager->startOperation(connectToSystemCallback);
        loginToCloud(uri.authenticator(), false, operationId);
    }
    else if (!auth.user.isEmpty() && !auth.password.isEmpty())
        connectToSystemCallback(true);
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

void QnMobileClientUriHandler::handleUrl(const nx::utils::Url& url)
{
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
