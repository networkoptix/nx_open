#include "camera_advanced_settings_widget.h"
#include "ui_camera_advanced_settings_widget.h"

#include <QtNetwork/QNetworkReply>

#include <api/app_server_connection.h>

#include <api/network_proxy_factory.h>

#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource/param.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_data_pool.h>

#include <ui/common/aligner.h>
#include <ui/widgets/properties/camera_advanced_settings_web_page.h>

#include <vms_gateway_embeddable.h>

namespace {

bool isStatusValid(Qn::ResourceStatus status)
{
    return status == Qn::Online || status == Qn::Recording;
}

}

QnCameraAdvancedSettingsWidget::QnCameraAdvancedSettingsWidget(QWidget* parent /* = 0*/):
    base_type(parent),
    ui(new Ui::CameraAdvancedSettingsWidget),
    m_page(Page::Empty)
{
    ui->setupUi(this);

    ui->cameraIdInputField->setTitle(tr("Camera ID"));
    ui->cameraIdInputField->setReadOnly(true);

    ui->primaryStreamUrlInputField->setTitle(tr("Primary Stream"));
    ui->primaryStreamUrlInputField->setReadOnly(true);
    ui->primaryStreamUrlInputField->setPlaceholderText(
        tr("URL is not available. Open video stream and try again"));

    ui->secondaryStreamUrlInputField->setTitle(tr("Secondary Stream"));
    ui->secondaryStreamUrlInputField->setReadOnly(true);
    ui->secondaryStreamUrlInputField->setPlaceholderText(
        tr("URL is not available. Open video stream and try again"));

    QnAligner* aligner = new QnAligner(this);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
    aligner->addWidgets({
        ui->cameraIdInputField,
        ui->primaryStreamUrlInputField,
        ui->secondaryStreamUrlInputField
    });

    initWebView();

    connect(ui->cameraAdvancedParamsWidget, 
        &QnCameraAdvancedParamsWidget::hasChangesChanged, 
        this, 
        &QnCameraAdvancedSettingsWidget::hasChangesChanged);
}

QnCameraAdvancedSettingsWidget::~QnCameraAdvancedSettingsWidget()
{
}

QnVirtualCameraResourcePtr QnCameraAdvancedSettingsWidget::camera() const
{
    return m_camera;
}

void QnCameraAdvancedSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_camera == camera)
        return;

    {
        QnMutexLocker locker(&m_cameraMutex);
        m_camera = camera;
    }

    m_cameraAdvancedSettingsWebPage->setCamera(m_camera);
    ui->cameraAdvancedParamsWidget->setCamera(m_camera);
}

void QnCameraAdvancedSettingsWidget::updateFromResource()
{
    updatePage();
    updateUrls();
}

void QnCameraAdvancedSettingsWidget::setPage(Page page)
{
    if (m_page == page)
        return;

    m_page = page;

    auto widgetByPage = [this]()-> QWidget*
    {
        switch (m_page)
        {
            case Page::Empty:
                return ui->noSettingsPage;
            case Page::Manual:
                return ui->manualPage;
            case Page::Web:
                return ui->webPage;
        }
        return nullptr;
    };

    ui->stackedWidget->setCurrentWidget(widgetByPage());
}


void QnCameraAdvancedSettingsWidget::updatePage()
{

    auto calculatePage = [this]
        {
            if (!m_camera)
                return Page::Empty;

            QnResourceData resourceData = qnCommon->dataPool()->data(m_camera);
            bool hasWebPage = resourceData.value<bool>(lit("showUrl"), false);
            if (hasWebPage)
                return Page::Web;

            if (!m_camera->getProperty(Qn::CAMERA_ADVANCED_PARAMETERS).isEmpty())
                return Page::Manual;

            return Page::Empty;
        };

    Page newPage = calculatePage();
    setPage(newPage);
}

void QnCameraAdvancedSettingsWidget::updateUrls()
{
    if (!m_camera)
    {
        ui->cameraIdInputField->setText(QString());
        ui->primaryStreamUrlInputField->setText(QString());
        ui->secondaryStreamUrlInputField->setText(QString());
        ui->secondaryStreamUrlInputField->setEnabled(true);
    }
    else
    {
        ui->cameraIdInputField->setText(m_camera->getId().toSimpleString());
        ui->primaryStreamUrlInputField->setText(
            m_camera->sourceUrl(Qn::CR_LiveVideo));
        ui->secondaryStreamUrlInputField->setEnabled(
            m_camera->hasDualStreaming2());
        if (m_camera->hasDualStreaming2())
        {
            ui->secondaryStreamUrlInputField->setText(
                m_camera->sourceUrl(Qn::CR_SecondaryLiveVideo));
        }
        else
        {
            ui->secondaryStreamUrlInputField->setText(
                tr("Camera has no secondary stream"));
        }
    }
}

void QnCameraAdvancedSettingsWidget::reloadData()
{
    updatePage();

    if (!m_camera || !isStatusValid(m_camera->getStatus()))
        return;

    if (m_page == Page::Web)
    {
        // QUrl doesn't work if it isn't constructed from QString and uses credentials.
        // It stays invalid with error code 'AuthorityPresentAndPathIsRelative'
        //  --rvasilenko, Qt 5.2.1
        const auto currentServerUrl = QnAppServerConnectionFactory::url();
        QnResourceData resourceData = qnCommon->dataPool()->data(m_camera);
        QUrl targetUrl = QString(lit("http://%1:%2/%3"))
            .arg(currentServerUrl.host())
            .arg(currentServerUrl.port())
            .arg(resourceData.value<QString>(lit("urlLocalePath"), QString()));

        QAuthenticator auth = m_camera->getAuth();
        targetUrl.setUserName(auth.user());
        targetUrl.setPassword(auth.password());

        auto gateway = nx::cloud::gateway::VmsGatewayEmbeddable::instance();
        if (gateway->isSslEnabled())
        {
            gateway->enforceSslFor(
                SocketAddress(currentServerUrl.host(), currentServerUrl.port()),
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
    else if (m_page == Page::Manual)
    {
        ui->cameraAdvancedParamsWidget->loadValues();
    }
}

bool QnCameraAdvancedSettingsWidget::hasChanges() const
{
    if (ui->stackedWidget->currentWidget() != ui->manualPage)
        return false;

    return ui->cameraAdvancedParamsWidget->hasChanges();
}

void QnCameraAdvancedSettingsWidget::submitToResource()
{
    if (!hasChanges())
        return;

    ui->cameraAdvancedParamsWidget->saveValues();
}

void QnCameraAdvancedSettingsWidget::hideEvent(QHideEvent *event)
{
    Q_UNUSED(event);
    if (m_page != Page::Web)
        return;

    ui->webView->triggerPageAction(QWebPage::Stop);
    ui->webView->triggerPageAction(QWebPage::StopScheduledPageRefresh);
    ui->webView->setContent(tr("Loading...").toUtf8());
}

void QnCameraAdvancedSettingsWidget::initWebView()
{
    m_cameraAdvancedSettingsWebPage = new CameraAdvancedSettingsWebPage(ui->webView);
    ui->webView->setPage(m_cameraAdvancedSettingsWebPage);

    ui->webView->setContent(tr("Loading...").toUtf8());

    QStyle* style = QStyleFactory().create(lit("fusion"));
    ui->webView->setStyle(style);

    QPalette palGreenHlText = this->palette();

    // Outline around the menu
    palGreenHlText.setColor(QPalette::Window, Qt::gray);
    palGreenHlText.setColor(QPalette::WindowText, Qt::black);

    palGreenHlText.setColor(QPalette::BrightText, Qt::gray);
    palGreenHlText.setColor(QPalette::BrightText, Qt::black);

    // combo button
    palGreenHlText.setColor(QPalette::Button, Qt::gray);
    palGreenHlText.setColor(QPalette::ButtonText, Qt::black);

    // combo menu
    palGreenHlText.setColor(QPalette::Base, Qt::gray);
    palGreenHlText.setColor(QPalette::Text, Qt::black);

    // tool tips
    palGreenHlText.setColor(QPalette::ToolTipBase, Qt::gray);
    palGreenHlText.setColor(QPalette::ToolTipText, Qt::black);

    palGreenHlText.setColor(QPalette::NoRole, Qt::gray);
    palGreenHlText.setColor(QPalette::AlternateBase, Qt::gray);

    palGreenHlText.setColor(QPalette::Link, Qt::black);
    palGreenHlText.setColor(QPalette::LinkVisited, Qt::black);

    // highlight button & menu
    palGreenHlText.setColor(QPalette::Highlight, Qt::gray);
    palGreenHlText.setColor(QPalette::HighlightedText, Qt::black);

    // to customize the disabled color
    palGreenHlText.setColor(QPalette::Disabled, QPalette::Button, Qt::gray);
    palGreenHlText.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::black);

    ui->webView->setPalette(palGreenHlText);

    ui->webView->setAutoFillBackground(true);

    ui->webView->setBackgroundRole(palGreenHlText.Base);
    ui->webView->setForegroundRole(palGreenHlText.Base);

    connect(ui->webView->page()->networkAccessManager(), &QNetworkAccessManager::sslErrors,
        this, [](QNetworkReply* reply, const QList<QSslError> &) { reply->ignoreSslErrors(); });
    connect(ui->webView->page()->networkAccessManager(), &QNetworkAccessManager::authenticationRequired,
        this, &QnCameraAdvancedSettingsWidget::at_authenticationRequired, Qt::DirectConnection);
    connect(ui->webView->page()->networkAccessManager(), &QNetworkAccessManager::proxyAuthenticationRequired,
        this, &QnCameraAdvancedSettingsWidget::at_proxyAuthenticationRequired, Qt::DirectConnection);

    
}

void QnCameraAdvancedSettingsWidget::at_authenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator)
{
    Q_UNUSED(reply);

    QnMutexLocker locker(&m_cameraMutex);
    if (!m_camera)
        return;

    QAuthenticator auth = m_camera->getAuth();

    authenticator->setUser(auth.user());
    authenticator->setPassword(auth.password());
}

void QnCameraAdvancedSettingsWidget::at_proxyAuthenticationRequired(const QNetworkProxy& proxy, QAuthenticator* authenticator)
{
    Q_UNUSED(proxy);

    QnMutexLocker locker(&m_cameraMutex);
    if (!m_camera)
        return;

    QString user = QnAppServerConnectionFactory::url().userName();
    QString password = QnAppServerConnectionFactory::url().password();
    authenticator->setUser(user);
    authenticator->setPassword(password);
}
