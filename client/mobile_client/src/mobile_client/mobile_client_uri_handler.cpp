#include "mobile_client_uri_handler.h"

#include <nx/vms/utils/system_uri.h>
#include <utils/common/app_info.h>
#include <watchers/cloud_status_watcher.h>

#include "mobile_client_ui_controller.h"
#include "mobile_client_settings.h"

using nx::vms::utils::SystemUri;

QnMobileClientUriHandler::QnMobileClientUriHandler(QObject* parent) :
    QObject(parent),
    m_uiController(nullptr)
{
}

void QnMobileClientUriHandler::setUiController(QnMobileClientUiController* uiController)
{
    m_uiController = uiController;
}

QStringList QnMobileClientUriHandler::supportedSchemes()
{
    const auto protocols =
        {
            SystemUri::Protocol::Native,
            SystemUri::Protocol::Http,
            SystemUri::Protocol::Https
        };

    QStringList result;
    for (auto protocol: protocols)
        result.append(SystemUri::toString(protocol));

    return result;
}

const char*QnMobileClientUriHandler::handlerMethodName()
{
    return "handleUrl";
}

void QnMobileClientUriHandler::handleUrl(const QUrl& url)
{
    SystemUri uri(url);

    if (!uri.isValid())
        return;

    if (uri.protocol() != SystemUri::Protocol::Native
        && uri.domain() != QnAppInfo::defaultCloudHost())
    {
        if (uri.scope() == SystemUri::Scope::Generic)
        {
            // Re-call openUrl to let QDesktopServices open URL in browser.
            QDesktopServices::openUrl(url);
        }

        return;
    }

    switch (uri.clientCommand())
    {
        case SystemUri::ClientCommand::None:
            break;
        case SystemUri::ClientCommand::Client:
            // Do nothing because the app will be raised by OS whithout our help.
            break;
        case SystemUri::ClientCommand::ConnectToSystem:
            if (m_uiController)
            {
                const auto url = uri.connectionUrl();
                if (url.isValid())
                    m_uiController->connectToSystem(url);
            }
            break;
        case SystemUri::ClientCommand::LoginToCloud:
            if (m_uiController)
            {
                m_uiController->disconnectFromSystem();
                qnCloudStatusWatcher->setCloudCredentials(
                    uri.authenticator().user, uri.authenticator().password);
            }
            break;
    }
}
