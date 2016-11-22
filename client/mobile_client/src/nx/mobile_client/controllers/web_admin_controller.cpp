#include "web_admin_controller.h"

#include <QtGui/QDesktopServices>

#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_ui_controller.h>

namespace nx {
namespace mobile_client {
namespace controllers {

WebAdminController::WebAdminController(QObject* parent):
    QObject(parent)
{
}

void WebAdminController::setUiController(QnMobileClientUiController* controller)
{
    m_uiController = controller;
}

void WebAdminController::openUrlInBrowser(const QString& urlString)
{
    QDesktopServices::openUrl(urlString);
}

QString WebAdminController::getCredentials() const
{
    return QString();
}

void WebAdminController::updateCredentials(
    const QString& login, const QString& password, bool isCloud)
{
}

void WebAdminController::cancel()
{
    // Nothing to do for lite client. Webadmin should just come to its initial state.
    // THe method is here only for compatibility with QnSetupWizardDialog.
}

bool WebAdminController::liteClientMode() const
{
    return qnSettings->isLiteClientModeEnabled();
}

void WebAdminController::startCamerasMode()
{
    if (!m_uiController)
        return;

    m_uiController->openResourcesScreen();
}

} // namespace controllers
} // namespace mobile_client
} // namespace nx
