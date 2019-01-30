#include "legacy_advanced_settings_widget.h"
#include "ui_legacy_advanced_settings_widget.h"

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QNetworkReply>
#include <QtGui/QContextMenuEvent>
#include <QtWebKitWidgets/QWebFrame>

#include <api/app_server_connection.h>
#include <api/network_proxy_factory.h>
#include <common/static_common_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/param.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_data_pool.h>
#include <ui/style/webview_style.h>
#include <utils/xml/camera_advanced_param_reader.h>

#include <nx/cloud/vms_gateway/vms_gateway_embeddable.h>
#include <nx/vms/client/desktop/resource_properties/camera/camera_advanced_settings_web_page.h>

namespace nx::vms::client::desktop {

LegacyAdvancedSettingsWidget::LegacyAdvancedSettingsWidget(QWidget* parent /* = 0*/):
    base_type(parent),
    ui(new Ui::LegacyAdvancedSettingsWidget)
{
    ui->setupUi(this);

    initWebView();

    connect(ui->cameraAdvancedParamsWidget,
        &CameraAdvancedParamsWidget::hasChangesChanged,
        this,
        &LegacyAdvancedSettingsWidget::hasChangesChanged);

    connect(ui->streamsPanel,
        &nx::vms::client::desktop::LegacyCameraSettingsStreamsPanel::hasChangesChanged,
        this,
        &LegacyAdvancedSettingsWidget::hasChangesChanged);
}

LegacyAdvancedSettingsWidget::~LegacyAdvancedSettingsWidget() = default;

QnVirtualCameraResourcePtr LegacyAdvancedSettingsWidget::camera() const
{
    return m_camera;
}

void LegacyAdvancedSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_camera == camera)
        return;

    if (m_camera)
        m_camera->disconnect(this);

    {
        QnMutexLocker locker(&m_cameraMutex);
        m_camera = camera;
    }

    if (m_camera)
    {
        connect(m_camera, &QnResource::statusChanged, this,
            &LegacyAdvancedSettingsWidget::updatePage);
    }

    m_cameraAdvancedSettingsWebPage->setCamera(m_camera);
    ui->cameraAdvancedParamsWidget->setCamera(m_camera);
    ui->streamsPanel->setCamera(m_camera);
}

void LegacyAdvancedSettingsWidget::updateFromResource()
{
    updatePage();
    ui->streamsPanel->updateFromResource();

    const bool isIoModule = m_camera && m_camera->isIOModule();

    ui->noSettingsLabel->setText(isIoModule
        ? tr("This I/O module has no advanced settings")
        : tr("This camera has no advanced settings"));
}


bool LegacyAdvancedSettingsWidget::hasManualPage() const
{
    if (!m_camera)
        return false;

    const auto& params = QnCameraAdvancedParamsReader::paramsFromResource(m_camera);
    if (params.groups.empty())
        return false;

     return m_camera->isOnline()
        || ui->cameraAdvancedParamsWidget->hasItemsAvailableInOffline();
}

bool LegacyAdvancedSettingsWidget::hasWebPage() const
{
    if (!m_camera || !m_camera->isOnline())
        return false;
    QnResourceData resourceData = m_camera->commonModule()->dataPool()->data(m_camera);
    return resourceData.value<bool>(lit("showUrl"), false);
}

void LegacyAdvancedSettingsWidget::updatePage()
{

    while (ui->tabWidget->count() > 0)
        ui->tabWidget->removeTab(0);

    if (hasManualPage())
        ui->tabWidget->addTab(ui->manualPage, tr("Settings"));
    if (hasWebPage())
        ui->tabWidget->addTab(ui->webPage, tr("Web"));
    if (ui->tabWidget->count() == 0)
        ui->tabWidget->addTab(ui->noSettingsPage, tr("No settings"));
}


void LegacyAdvancedSettingsWidget::reloadData()
{
    updatePage();

    if (hasWebPage())
    {
        NX_ASSERT(m_camera);
        if (!m_camera)
            return;

        QnResourceData resourceData = m_camera->commonModule()->dataPool()->data(m_camera);
        auto urlPath = resourceData.value<QString>(lit("urlLocalePath"), QString());
        while (urlPath.startsWith(lit("/")))
            urlPath = urlPath.mid(1); //< VMS Gateway does not like several slashes at the beginning.

        // QUrl doesn't work if it isn't constructed from QString and uses credentials.
        // It stays invalid with error code 'AuthorityPresentAndPathIsRelative'
        //  --rvasilenko, Qt 5.2.1
        const auto currentServerUrl = commonModule()->currentUrl();
        QUrl targetUrl(lm(lit("http://%1:%2/%3")).args(
            currentServerUrl.host(), currentServerUrl.port(), urlPath));

        QAuthenticator auth = m_camera->getAuth();
        targetUrl.setUserName(auth.user());
        targetUrl.setPassword(auth.password());

        auto gateway = nx::cloud::gateway::VmsGatewayEmbeddable::instance();
        if (gateway->isSslEnabled())
        {
            gateway->enforceSslFor(
                nx::network::SocketAddress(currentServerUrl.host(), currentServerUrl.port()),
                currentServerUrl.scheme() == lit("https"));
        }

        const auto gatewayAddress = gateway->endpoint();
        const QNetworkProxy gatewayProxy(
            QNetworkProxy::HttpProxy,
            gatewayAddress.address.toString(), gatewayAddress.port,
            currentServerUrl.userName(), currentServerUrl.password());

        m_cameraAdvancedSettingsWebPage->networkAccessManager()->setProxy(gatewayProxy);

        ui->webView->reload();
        ui->webView->load(QNetworkRequest(targetUrl));
        ui->webView->show();
    }

    if (hasManualPage())
    {
        ui->cameraAdvancedParamsWidget->loadValues();
    }
}

bool LegacyAdvancedSettingsWidget::hasChanges() const
{
    return ui->streamsPanel->hasChanges()
        || (hasManualPage() && ui->cameraAdvancedParamsWidget->hasChanges());
}

void LegacyAdvancedSettingsWidget::submitToResource()
{
    if (!hasChanges())
        return;

    ui->cameraAdvancedParamsWidget->saveValues();
    ui->streamsPanel->submitToResource();
}

void LegacyAdvancedSettingsWidget::hideEvent(QHideEvent *event)
{
    Q_UNUSED(event);
    if (!hasWebPage())
        return;

    ui->webView->triggerPageAction(QWebPage::Stop);
    ui->webView->triggerPageAction(QWebPage::StopScheduledPageRefresh);
    ui->webView->setContent(tr("Loading...").toUtf8());
}

bool LegacyAdvancedSettingsWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != ui->webView || event->type() != QEvent::ContextMenu)
        return base_type::eventFilter(watched, event);

    const auto page = ui->webView->page();
    if (!page)
        return base_type::eventFilter(watched, event);

    const auto frame = page->mainFrame();
    if (!frame)
        return base_type::eventFilter(watched, event);

    const auto menuEvent = dynamic_cast<QContextMenuEvent*>(event);
    if (!menuEvent)
        return base_type::eventFilter(watched, event);

    const auto hitTest = frame->hitTestContent(menuEvent->pos());

    const auto policy = hitTest.linkUrl().isEmpty()
        ? Qt::DefaultContextMenu
        : Qt::ActionsContextMenu; //< Special context menu for links.

    ui->webView->setContextMenuPolicy(policy);
    return base_type::eventFilter(watched, event);
}

void LegacyAdvancedSettingsWidget::initWebView()
{
    NxUi::setupWebViewStyle(ui->webView);

    ui->webView->installEventFilter(this);

    m_cameraAdvancedSettingsWebPage = new CameraAdvancedSettingsWebPage(ui->webView);
    ui->webView->setPage(m_cameraAdvancedSettingsWebPage);

    // Special actions list for context menu for links.
    ui->webView->insertActions(nullptr, QList<QAction*>()
       << ui->webView->pageAction(QWebPage::OpenLink)
       << ui->webView->pageAction(QWebPage::Copy)
       << ui->webView->pageAction(QWebPage::CopyLinkToClipboard));

    ui->webView->setContent(tr("Loading...").toUtf8());

    connect(ui->webView->page()->networkAccessManager(), &QNetworkAccessManager::sslErrors,
        this, [](QNetworkReply* reply, const QList<QSslError> &) { reply->ignoreSslErrors(); });
    connect(ui->webView->page()->networkAccessManager(), &QNetworkAccessManager::authenticationRequired,
        this, &LegacyAdvancedSettingsWidget::at_authenticationRequired, Qt::DirectConnection);
    connect(ui->webView->page()->networkAccessManager(), &QNetworkAccessManager::proxyAuthenticationRequired,
        this, &LegacyAdvancedSettingsWidget::at_proxyAuthenticationRequired, Qt::DirectConnection);
}

void LegacyAdvancedSettingsWidget::at_authenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator)
{
    Q_UNUSED(reply);

    QnMutexLocker locker(&m_cameraMutex);
    if (!m_camera)
        return;

    QAuthenticator auth = m_camera->getAuth();

    authenticator->setUser(auth.user());
    authenticator->setPassword(auth.password());
}

void LegacyAdvancedSettingsWidget::at_proxyAuthenticationRequired(const QNetworkProxy& proxy, QAuthenticator* authenticator)
{
    Q_UNUSED(proxy);

    QnMutexLocker locker(&m_cameraMutex);
    if (!m_camera)
        return;

    QString user = commonModule()->currentUrl().userName();
    QString password = commonModule()->currentUrl().password();
    authenticator->setUser(user);
    authenticator->setPassword(password);
}

} // namespace nx::vms::client::desktop
