#include "mobile_client_uri_handler.h"

#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/vms/utils/system_uri.h>
#include <watchers/cloud_status_watcher.h>

#include "mobile_client_ui_controller.h"
#include "mobile_client_settings.h"

using nx::vms::utils::SystemUri;

struct QnMobileClientUriHandler::Private
{
    void handleLogInToCloudCommand(const SystemUri& uri);
    void handleClientCommand(const SystemUri& uri);

    QPointer<QnMobileClientUiController> uiController;
};

void QnMobileClientUriHandler::Private::handleLogInToCloudCommand(const SystemUri& uri)
{
    if (!uiController || uri.clientCommand() != SystemUri::ClientCommand::LoginToCloud)
        return;

    const auto user = uri.authenticator().user;
    const auto password = uri.authenticator().password;
    if (!user.isEmpty() && !password.isEmpty())
    {
        uiController->disconnectFromSystem();
        qnCloudStatusWatcher->setCredentials(QnEncodedCredentials(user, password));
        return;
    }

    if (user.isEmpty())
    {
        if (!password.isEmpty())
        {
            NX_EXPECT(false, "We dont support password without user");
        }
        else if (qnCloudStatusWatcher->status() == QnCloudStatusWatcher::LoggedOut)
        {
            uiController->disconnectFromSystem();
            uiController->openLoginToCloudScreen(user, password);
        }
        return;
    }

    if (user == qnCloudStatusWatcher->credentials().user)
        return; // We logged in already.

    // We don't reset current cloud credentials to give user opportunity to cancel requested
    // login to cloud action.
    uiController->disconnectFromSystem();
    uiController->openLoginToCloudScreen(user, password);
}

void QnMobileClientUriHandler::Private::handleClientCommand(const SystemUri& uri)
{
    if (!uiController || uri.systemId().isEmpty()
        || uri.clientCommand() != SystemUri::ClientCommand::Client)
    {
        return;
    }

    uiController->disconnectFromSystem();
    if (!uri.authenticator().user.isEmpty() && !uri.authenticator().password.isEmpty())
    {
        qnCloudStatusWatcher->setCredentials(QnEncodedCredentials(
            uri.authenticator().user, uri.authenticator().password));
    }

    const auto url = uri.connectionUrl();
    if (url.isValid())
        uiController->connectToSystem(url);
}

//-------------------------------------------------------------------------------------------------

QnMobileClientUriHandler::QnMobileClientUriHandler(QObject* parent) :
    QObject(parent),
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

    switch (uri.clientCommand())
    {
        case SystemUri::ClientCommand::None:
            break;
        case SystemUri::ClientCommand::Client:
            d->handleClientCommand(uri);
            break;
        case SystemUri::ClientCommand::LoginToCloud:
            d->handleLogInToCloudCommand(uri);
            break;
        case SystemUri::ClientCommand::OpenOnPortal:
            break;
    }
}
