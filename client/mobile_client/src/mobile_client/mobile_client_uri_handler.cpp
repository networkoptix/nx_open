#include "mobile_client_uri_handler.h"

#include <nx/vms/utils/system_uri.h>
#include <nx/vms/utils/app_info.h>
#include <utils/common/app_info.h>
#include <watchers/cloud_status_watcher.h>

#include "mobile_client_ui_controller.h"
#include "mobile_client_settings.h"

namespace {

QUrl getConnectionUrl(const nx::vms::utils::SystemUri& uri)
{
    if (!uri.isValid() || uri.systemId().isEmpty())
        return QUrl();

    const auto split = uri.systemId().splitRef(L':', QString::SkipEmptyParts);
    if (split.size() > 2)
        return QUrl();

    int port = -1;
    if (split.size() == 2)
    {
        bool ok;
        port = split.last().toInt(&ok);
        if (!ok)
            return QUrl();
    }

    QUrl result;
    result.setScheme(lit("http"));
    result.setHost(split.first().toString());
    result.setPort(port);
    result.setUserName(uri.authenticator().user);
    result.setPassword(uri.authenticator().password);
    return result;
}

} // namespace

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
    if (!uri.isValid() ||
        (uri.protocol() != SystemUri::Protocol::Native && uri.domain() != QnAppInfo::defaultCloudHost()))
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
                m_uiController->connectToSystem(getConnectionUrl(uri));
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
