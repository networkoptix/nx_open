#include "camera_advanced_settings_widget.h"
#include "ui_camera_advanced_settings_widget.h"

#include <QtNetwork/QNetworkReply>

#include <api/app_server_connection.h>
#include <api/media_server_connection.h>
#include <api/network_proxy_factory.h>

#include <common/common_module.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/param.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_data_pool.h>

#include <ui/widgets/properties/camera_advanced_settings_web_page.h>

#include <utils/camera_advanced_settings_xml_parser.h>

QnCameraAdvancedSettingsWidget::QnCameraAdvancedSettingsWidget(QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::CameraAdvancedSettingsWidget),
    m_page(Page::Empty),
    m_widgetsRecreator(NULL)
{
    ui->setupUi(this);

    QList<int> sizes = ui->splitter->sizes();
    sizes[0] = 200;
    sizes[1] = 400;
    ui->splitter->setSizes(sizes);
    
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
    //ui->webView->setStyleSheet(lit("background-color: rgb(255, 255, 255); color: rgb(255, 255, 255)"));

    ui->webView->setBackgroundRole(palGreenHlText.Base);
    ui->webView->setForegroundRole(palGreenHlText.Base);

    connect(ui->webView->page()->networkAccessManager(), &QNetworkAccessManager::sslErrors,
        this, [](QNetworkReply* reply, const QList<QSslError> &){reply->ignoreSslErrors();} );
    connect(ui->webView->page()->networkAccessManager(), &QNetworkAccessManager::authenticationRequired,
        this, &QnCameraAdvancedSettingsWidget::at_authenticationRequired, Qt::DirectConnection );
    connect(ui->webView->page()->networkAccessManager(), &QNetworkAccessManager::proxyAuthenticationRequired,
        this, &QnCameraAdvancedSettingsWidget::at_proxyAuthenticationRequired, Qt::DirectConnection);

}

QnCameraAdvancedSettingsWidget::~QnCameraAdvancedSettingsWidget() {
    clean();
}

const QnVirtualCameraResourcePtr & QnCameraAdvancedSettingsWidget::camera() const {
    return m_camera;
}

void QnCameraAdvancedSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera) {
    if (m_camera == camera)
        return;

    QMutexLocker locker(&m_cameraMutex);
    m_camera = camera;
    m_cameraAdvancedSettingsWebPage->setCamera(m_camera);      
}

void QnCameraAdvancedSettingsWidget::updateFromResource() {
    clean();
    updatePage();
}

void QnCameraAdvancedSettingsWidget::clean() {
    if (m_widgetsRecreator) {
        delete m_widgetsRecreator;
        m_widgetsRecreator = NULL;
    }
}

void QnCameraAdvancedSettingsWidget::refresh() {
    qDebug() << "asked for refresh, WHY";
    return;

    clean();
    reloadData();
}

void QnCameraAdvancedSettingsWidget::createWidgetsRecreator(const QString &cameraUniqueId, const QString &paramId) {
    clean();

    QStackedLayout* stackedLayout = qobject_cast<QStackedLayout*>(ui->propertiesStackedWidget->layout());
    Q_ASSERT(stackedLayout);

    m_widgetsRecreator = new CameraSettingsWidgetsTreeCreator(cameraUniqueId, paramId,  ui->webView, ui->treeWidget, stackedLayout);
    m_widgetsRecreator->setCamera( m_camera );

    connect(m_widgetsRecreator, &CameraSettingsWidgetsTreeCreator::advancedParamChanged, this, &QnCameraAdvancedSettingsWidget::at_advancedParamChanged);
    connect(m_widgetsRecreator, &CameraSettingsWidgetsTreeCreator::refreshAdvancedSettings, this, &QnCameraAdvancedSettingsWidget::refresh, Qt::QueuedConnection);
}

void QnCameraAdvancedSettingsWidget::updatePage() {
    
    auto getPage = [this]{
        if (!m_camera)
            return Page::Empty;
        
        QnResourceData resourceData = qnCommon->dataPool()->data(m_camera);
        bool hasWebPage = resourceData.value<bool>(lit("showUrl"), false);
        if (hasWebPage) 
            return Page::Web;

        if (!m_camera->getProperty( Qn::CAMERA_SETTINGS_ID_PARAM_NAME ).isNull()) 
            return Page::Manual;
        
        return Page::Empty;
    };

    Page newPage = getPage();
    if (m_page == newPage)
        return;

    m_page = newPage;

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

void QnCameraAdvancedSettingsWidget::reloadData() {
    updatePage();

    if (!m_camera || m_camera->getStatus() != Qn::Online)
        return;

    if (m_page == QnCameraAdvancedSettingsWidget::Page::Manual) {
        QnMediaServerConnectionPtr serverConnection = getServerConnection();
        if (!serverConnection)
            return;

        QString id = m_camera->getProperty( Qn::CAMERA_SETTINGS_ID_PARAM_NAME );
        createWidgetsRecreator(m_camera->getUniqueId(), id);

        //TODO #ak remove this XML parsing run      
        CameraSettingsTreeLister lister( id, m_camera );
        QStringList settings = lister.proceed();

        serverConnection->getParamsAsync(m_camera, settings, this, SLOT(at_advancedSettingsLoaded(int, const QnStringVariantPairList &, int)) );
    } else if (m_page == QnCameraAdvancedSettingsWidget::Page::Web) {
        QnResourceData resourceData = qnCommon->dataPool()->data(m_camera);
        m_lastCameraPageUrl = QString(QLatin1String("http://%1:%2/%3")).
            arg(m_camera->getHostAddress()).arg(m_camera->httpPort()).
            arg(resourceData.value<QString>(lit("urlLocalePath"), QString()));
        m_lastCameraPageUrl.setUserName( m_camera->getAuth().user() );
        m_lastCameraPageUrl.setPassword( m_camera->getAuth().password() );
        m_cameraAdvancedSettingsWebPage->networkAccessManager()->setProxy(QnNetworkProxyFactory::instance()->proxyToResource(m_camera));

        ui->webView->reload();
        ui->webView->load( QNetworkRequest(m_lastCameraPageUrl) );
        ui->webView->show();
    }
}

QnMediaServerConnectionPtr QnCameraAdvancedSettingsWidget::getServerConnection() const {
    if (!m_camera)
        return QnMediaServerConnectionPtr();

    if (QnMediaServerResourcePtr mediaServer = m_camera->getParentResource().dynamicCast<QnMediaServerResource>())
        return mediaServer->apiConnection();

    return QnMediaServerConnectionPtr();
}

void QnCameraAdvancedSettingsWidget::at_authenticationRequired(QNetworkReply* /*reply*/, QAuthenticator * authenticator)
{
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

void QnCameraAdvancedSettingsWidget::at_advancedSettingsLoaded(int status, const QnStringVariantPairList &params, int handle) {
    Q_UNUSED(handle);

    if (!m_widgetsRecreator) {
        qWarning() << "QnSingleCameraSettingsWidget::at_advancedSettingsLoaded: widgets creator ptr is null, camera id: "
            << (m_camera == 0? lit("unknown"): m_camera->getUniqueId());
        return;
    }
    if (status != 0) {
        qWarning() << "QnSingleCameraSettingsWidget::at_advancedSettingsLoaded: http status code is not OK: " << status
            << ". Camera id: " << (m_camera == 0? lit("unknown"): m_camera->getUniqueId());
        return;
    }

    if (!m_camera || m_camera->getUniqueId() != m_widgetsRecreator->getCameraId()) {
        //If so, we received update for some other camera
        return;
    }

    CameraSettings cameraSettings;
    for (auto it = params.cbegin(); it != params.cend(); ++it) {
        QString val = it->second.toString();
        if (val.isEmpty())
            continue;

        CameraSettingPtr tmp(new CameraSetting());
        tmp->deserializeFromStr(val);

        CameraSettings::Iterator sIt = cameraSettings.find(tmp->getId());
        if (sIt == cameraSettings.end()) {
            cameraSettings.insert(tmp->getId(), tmp);
            continue;
        }

        CameraSetting& savedVal = *(sIt.value());
        if (CameraSettingReader::isEnabled(savedVal)) {
            CameraSettingValue newVal = tmp->getCurrent();
            if (savedVal.getCurrent() != newVal) {
                savedVal.setCurrent(newVal);
            }
            continue;
        }

        if (CameraSettingReader::isEnabled(*tmp)) {
            cameraSettings.erase(sIt);
            cameraSettings.insert(tmp->getId(), tmp);
            continue;
        }
    }

    
    m_widgetsRecreator->proceed(cameraSettings);
}

void QnCameraAdvancedSettingsWidget::at_advancedParam_saved(int httpStatusCode, const QnStringBoolPairList& operationResult) {
    qDebug() << "at_advancedParam_saved" << httpStatusCode << operationResult;

    QString error = httpStatusCode == 0 
        ? tr("Possibly, appropriate camera's service is unavailable now")
        : tr("Server returned the following error code : ") + httpStatusCode; 

    QString failedParams;
    for (auto it = operationResult.constBegin(); it != operationResult.constEnd(); ++it)
    {
        if (!it->second) {
            QString formattedParam(lit("Advanced->") + it->first.right(it->first.length() - 2));
            failedParams += lit("\n");
            failedParams += formattedParam.replace(lit("%%"), lit("->")); // TODO: #Elric #TR
        }
    }

    if (!failedParams.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Could not save parameters"),
            tr("Failed to save the following parameters (%1):\n%2").arg(error, failedParams)
            );

        refresh();
    }
}

void QnCameraAdvancedSettingsWidget::at_advancedParamChanged(const CameraSetting& val) {
    auto serverConnection = getServerConnection();
    if (!serverConnection)
        return;

    QnStringVariantPairList params;
    params << QPair<QString, QVariant>(val.getId(), QVariant(val.serializeToStr()));
    serverConnection->setParamsAsync(m_camera, params, this, SLOT(at_advancedParam_saved(int, const QnStringBoolPairList &)));
}
