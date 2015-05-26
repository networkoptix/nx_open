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

#include <ui/widgets/properties/camera_advanced_settings_web_page.h>

namespace {

    bool isStatusValid(Qn::ResourceStatus status) {
        return status == Qn::Online || status == Qn::Recording;
    }

}

QnCameraAdvancedSettingsWidget::QnCameraAdvancedSettingsWidget(QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::CameraAdvancedSettingsWidget),
    m_page(Page::Empty)
{
    ui->setupUi(this);
    initWebView();
}

QnCameraAdvancedSettingsWidget::~QnCameraAdvancedSettingsWidget() {
}

QnVirtualCameraResourcePtr QnCameraAdvancedSettingsWidget::camera() const {
    return m_camera;
}

void QnCameraAdvancedSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera) {
    if (m_camera == camera)
        return;

    QMutexLocker locker(&m_cameraMutex);
    m_camera = camera;
    m_cameraAdvancedSettingsWebPage->setCamera(m_camera);
    ui->cameraAdvancedParamsWidget->setCamera(m_camera);
}

void QnCameraAdvancedSettingsWidget::updateFromResource() {
    updatePage();
}

void QnCameraAdvancedSettingsWidget::setPage(Page page) {
    if (m_page == page)
        return;

    m_page = page;

    auto widgetByPage = [this] ()-> QWidget* {
        switch (m_page) {
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


void QnCameraAdvancedSettingsWidget::updatePage() {
    
    auto calculatePage = [this]{
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

void QnCameraAdvancedSettingsWidget::reloadData() {
    updatePage();

    if (!m_camera || !isStatusValid(m_camera->getStatus()))
        return;

    if (m_page == Page::Web) {
        QnResourceData resourceData = qnCommon->dataPool()->data(m_camera);
        QUrl targetUrl;
        targetUrl.setHost(m_camera->getHostAddress());
        targetUrl.setPort(m_camera->httpPort());
        targetUrl.setPath(resourceData.value<QString>(lit("urlLocalePath"), QString()));
        targetUrl.setUserName( m_camera->getAuth().user() );
        targetUrl.setPassword( m_camera->getAuth().password() );
        m_cameraAdvancedSettingsWebPage->networkAccessManager()->setProxy(QnNetworkProxyFactory::instance()->proxyToResource(m_camera));

        ui->webView->reload();
        ui->webView->load( QNetworkRequest(targetUrl) );
        ui->webView->show();
    } else if (m_page == Page::Manual) {
        ui->cameraAdvancedParamsWidget->loadValues();
    }
}

void QnCameraAdvancedSettingsWidget::initWebView() {
    m_cameraAdvancedSettingsWebPage = new CameraAdvancedSettingsWebPage(ui->webView);
    ui->webView->setPage(m_cameraAdvancedSettingsWebPage);

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
        this, [](QNetworkReply* reply, const QList<QSslError> &){reply->ignoreSslErrors();} );
    connect(ui->webView->page()->networkAccessManager(), &QNetworkAccessManager::authenticationRequired,
        this, &QnCameraAdvancedSettingsWidget::at_authenticationRequired, Qt::DirectConnection );
    connect(ui->webView->page()->networkAccessManager(), &QNetworkAccessManager::proxyAuthenticationRequired,
        this, &QnCameraAdvancedSettingsWidget::at_proxyAuthenticationRequired, Qt::DirectConnection);
}

void QnCameraAdvancedSettingsWidget::at_authenticationRequired(QNetworkReply* /*reply*/, QAuthenticator * authenticator) {
    QMutexLocker locker(&m_cameraMutex);
    if (!m_camera)
        return;

    authenticator->setUser(m_camera->getAuth().user());
    authenticator->setPassword(m_camera->getAuth().password());
}

void QnCameraAdvancedSettingsWidget::at_proxyAuthenticationRequired(const QNetworkProxy & , QAuthenticator * authenticator) {    
    QMutexLocker locker(&m_cameraMutex);
    if (!m_camera)
        return;

    QString user = QnAppServerConnectionFactory::url().userName();
    QString password = QnAppServerConnectionFactory::url().password();
    authenticator->setUser(user);
    authenticator->setPassword(password);
}
