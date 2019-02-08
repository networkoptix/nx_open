#include "web_admin_controller.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtGui/QDesktopServices>

#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_ui_controller.h>

namespace nx {
namespace mobile_client {
namespace controllers {

WebAdminController::WebAdminController(QObject* parent):
    QObject(parent),
    m_credentials(qnSettings->startupParameters().url)
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
    QJsonObject json;
    json["localLogin"] = m_credentials.user;
    json["localPassword"] = m_credentials.password;
    return QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact));
}

void WebAdminController::updateCredentials(
    const QString& login, const QString& password, bool /*isCloud*/)
{
    m_credentials = {login, password};
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
    m_uiController->connectToSystem(qnSettings->startupParameters().url);
}

} // namespace controllers
} // namespace mobile_client
} // namespace nx
