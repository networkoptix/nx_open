#include "mobile_client_uri_handler.h"

#include <nx/vms/utils/system_uri.h>
#include <nx/vms/utils/app_info.h>
#include <watchers/cloud_status_watcher.h>

#include "mobile_client_ui_controller.h"
#include "mobile_client_settings.h"

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
    return { nx::vms::utils::AppInfo::nativeUriProtocol(), lit("http"), lit("https") };
}

const char*QnMobileClientUriHandler::handlerMethodName()
{
    return "handleUrl";
}

void QnMobileClientUriHandler::handleUrl(const QUrl& url)
{
    using namespace nx::vms::utils;

    SystemUri uri(url);
    if (!uri.isValid())
    {
        if (uri.scope() == SystemUri::Scope::Generic)
        {
            // Recall openUrl to let QDesktopServices open URL in browser.
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
            break;
        case SystemUri::ClientCommand::LoginToCloud:
            if (m_uiController)
            {
                m_uiController->disconnectFromSystem();
//                qnCloudStatusWatcher->setCloudCredentials(uri.authenticator().user, uri.authenticator().password);
            }
            break;
    }
}
