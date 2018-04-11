#include "mobile_client_uri_handler.h"

#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/vms/utils/system_uri.h>
#include <watchers/cloud_status_watcher.h>
#include <nx/client/core/utils/async_operation_manager.h>

#include <common/common_module.h>
#include <client_core/client_core_module.h>

#include "mobile_client_ui_controller.h"
#include "mobile_client_settings.h"

using nx::vms::utils::SystemUri;
using nx::client::core::OperationManager;

//-------------------------------------------------------------------------------------------------

class QnMobileClientUriHandler::Private: public QObject
{
public:
    Private();

    void loginToCloud(
        const SystemUri::Auth& auth,
        bool forceLoginScreen = false,
        const QString& connectOperationId = QString());

    void handleClientCommand(const SystemUri& uri);

public:
    OperationManager* const operationManager;
    QPointer<QnMobileClientUiController> uiController;

private:
    void handleCloudStatusChanged();
    void loginToCloudDirectlyInternal(
        const SystemUri::Auth& auth,
        const QString& connectOperationId);

private:
    QString directCloudConnectionOperationId;
};

QnMobileClientUriHandler::Private::Private():
    operationManager(qnClientCoreModule->commonModule()->instance<OperationManager>())
{
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged,
        this, &Private::handleCloudStatusChanged);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::errorChanged,
        this, &Private::handleCloudStatusChanged);
}

void QnMobileClientUriHandler::Private::handleCloudStatusChanged()
{
    if (directCloudConnectionOperationId.isEmpty())
        return;

    const auto operationManager = qnClientCoreModule->commonModule()->instance<OperationManager>();
    const bool success = qnCloudStatusWatcher->status() == QnCloudStatusWatcher::Online;
    operationManager->finishOperation(directCloudConnectionOperationId, success);
}

void QnMobileClientUriHandler::Private::loginToCloudDirectlyInternal(
    const SystemUri::Auth& auth,
    const QString& connectOperationId)
{
    NX_EXPECT(directCloudConnectionOperationId.isEmpty(), "Can't have this value");

    uiController->disconnectFromSystem();
    const auto operationManager = qnClientCoreModule->commonModule()->instance<OperationManager>();

    const auto callback =
        [this, auth, connectOperationId](bool success)
        {
            directCloudConnectionOperationId.clear();
            if (!success)
                loginToCloud(auth, true, connectOperationId);
        };

    const auto credentials = QnEncodedCredentials(auth.user, auth.password);
    qnCloudStatusWatcher->setCredentials(credentials);
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
        && user == qnCloudStatusWatcher->cloudLogin()
        && password == qnCloudStatusWatcher->cloudPassword()
        && qnCloudStatusWatcher->status() == QnCloudStatusWatcher::Online;

    if (loggedInWithSameCredentials)
    {
        const auto operationManager =
            qnClientCoreModule->commonModule()->instance<OperationManager>();
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
        else if (qnCloudStatusWatcher->status() == QnCloudStatusWatcher::LoggedOut)
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

void QnMobileClientUriHandler::Private::handleClientCommand(const SystemUri& uri)
{
    if (!uiController || uri.clientCommand() != SystemUri::ClientCommand::Client)
        return;

    if (uri.systemId().isEmpty())
        return;

    const auto url = uri.connectionUrl();
    const auto auth = uri.authenticator();
    const bool cloudConnection = uri.hasCloudSystemId();
    const auto connectToSystemCallback =
        [url, cloudConnection, this](bool success)
        {
            uiController->disconnectFromSystem();
            if (!success || !url.isValid())
                return;

            auto targetUrl = url;
            if (cloudConnection)
            {
                targetUrl.setUserName(qnCloudStatusWatcher->cloudLogin());
                targetUrl.setPassword(qnCloudStatusWatcher->cloudPassword());
            }
            uiController->connectToSystem(targetUrl);
        };

    if (cloudConnection)
    {
        const auto operationId = operationManager->startOperation(connectToSystemCallback);
        loginToCloud(uri.authenticator(), false, operationId);
    }
    else if (!auth.user.isEmpty() && !auth.password.isEmpty())
        connectToSystemCallback(true);
}

//-------------------------------------------------------------------------------------------------

QnMobileClientUriHandler::QnMobileClientUriHandler(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

QnMobileClientUriHandler::~QnMobileClientUriHandler()
{
}

void QnMobileClientUriHandler::setUiController(QnMobileClientUiController* uiController)
{
    d->uiController = uiController;
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
