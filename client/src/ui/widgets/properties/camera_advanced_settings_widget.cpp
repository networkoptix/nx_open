#include "camera_advanced_settings_widget.h"
#include "ui_camera_advanced_settings_widget.h"

#include <QtNetwork/QNetworkReply>

#include <api/app_server_connection.h>
#include <api/media_server_connection.h>
#include <api/network_proxy_factory.h>

#include <common/common_module.h>

#include <core/resource/resource.h>
#include <core/resource/camera_advanced_param.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/param.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_data_pool.h>

#include <ui/style/warning_style.h>
#include <ui/widgets/properties/camera_advanced_settings_web_page.h>
#include <ui/widgets/properties/camera_advanced_param_widgets_manager.h>

#include <utils/common/model_functions.h>

namespace {

    bool isStatusValid(Qn::ResourceStatus status) {
        return status == Qn::Online || status == Qn::Recording;
    }

	bool parameterRequired(const QnCameraAdvancedParameter &param) {
		return param.isValid()
			&& param.dataType != QnCameraAdvancedParameter::DataType::Button
			&& param.dataType != QnCameraAdvancedParameter::DataType::ControlButtonsPair;
	}

	QSet<QString> parameterIds(const QnCameraAdvancedParamGroup& group) {
		QSet<QString> result;
		for (const QnCameraAdvancedParamGroup &group: group.groups)
			result.unite(parameterIds(group));
		for (const QnCameraAdvancedParameter &param: group.params)
			if (parameterRequired(param))
				result.insert(param.getId());
		return result;
	}

	QSet<QString> parameterIds(const QnCameraAdvancedParams& params) {
		QSet<QString> result;
		for (const QnCameraAdvancedParamGroup &group: params.groups)
			result.unite(parameterIds(group));
		return result;
	}

}

QnCameraAdvancedSettingsWidget::QnCameraAdvancedSettingsWidget(QWidget* parent /*= 0*/):
    base_type(parent),
    ui(new Ui::CameraAdvancedSettingsWidget),
	m_advancedParamsReader(new QnCameraAdvancedParamsReader()),
    m_page(Page::Empty),
    m_paramRequestHandle(0),
    m_applyingParamsCount(0)
{
    ui->setupUi(this);
	m_advancedParamWidgetsManager.reset(new QnCameraAdvancedParamWidgetsManager(ui->treeWidget, ui->propertiesStackedWidget));
	connect(m_advancedParamWidgetsManager, &QnCameraAdvancedParamWidgetsManager::paramValueChanged, this, &QnCameraAdvancedSettingsWidget::at_advancedParamChanged);

    QList<int> sizes = ui->splitter->sizes();
    sizes[0] = 200;
    sizes[1] = 400;
    ui->splitter->setSizes(sizes);

    initWebView();

    ui->manualLoadingWidget->setText(tr("Please wait while settings are being loaded.\nThis can take a lot of time."));
    setWarningStyle(ui->manualWarningLabel);
    updateApplyingParamsLabel();
}

QnCameraAdvancedSettingsWidget::~QnCameraAdvancedSettingsWidget() {
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
        case Page::CannotLoad:
            return ui->cannotLoadPage;
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

        if (!isStatusValid(m_camera->getStatus()))
            return Page::CannotLoad;
        
        QnResourceData resourceData = qnCommon->dataPool()->data(m_camera);
        bool hasWebPage = resourceData.value<bool>(lit("showUrl"), false);
        if (hasWebPage) 
            return Page::Web;

        if (!m_camera->getProperty( Qn::CAMERA_SETTINGS_ID_PARAM_NAME ).isNull()) 
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

    if (m_page == QnCameraAdvancedSettingsWidget::Page::Manual) {
		auto params = m_advancedParamsReader->params(m_camera);
		m_advancedParamWidgetsManager->displayParams(params);

		QStringList paramIds = parameterIds(params).toList();
		ui->manualSettingsWidget->setCurrentWidget(ui->manualContentPage);

		/* Nothing to request. */
		if (paramIds.isEmpty())
			return;

        QnMediaServerConnectionPtr serverConnection = getServerConnection();
        if (!serverConnection)
            return;
		
        ui->manualSettingsWidget->setCurrentWidget(ui->manualLoadingPage);
        m_paramRequestHandle = serverConnection->getParamsAsync(m_camera, paramIds, this, SLOT(at_advancedSettingsLoaded(int, const QnCameraAdvancedParamValueList &, int)) );
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


void QnCameraAdvancedSettingsWidget::updateApplyingParamsLabel() {
    ui->manualApplyingProgressWidget->setVisible(m_applyingParamsCount > 0);
    ui->manualApplyingProgressWidget->setText(tr("Applying settings..."));
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

void QnCameraAdvancedSettingsWidget::at_advancedSettingsLoaded(int status, const QnCameraAdvancedParamValueList &params, int handle) {
    if (handle != m_paramRequestHandle)
        return;

    ui->manualSettingsWidget->setCurrentWidget(ui->manualContentPage);

    if (params.isEmpty() || status != 0) {
        qWarning() << "QnSingleCameraSettingsWidget::at_advancedSettingsLoaded: http status code is not OK: " << status
            << ". Camera id: " << (m_camera == 0? lit("unknown"): m_camera->getUniqueId());
		setPage(Page::CannotLoad);
        return;
    }
	
	m_advancedParamWidgetsManager->loadValues(params);

}

void QnCameraAdvancedSettingsWidget::at_advancedParam_saved(int status, const QnCameraAdvancedParamValueList &params, int handle) {
    --m_applyingParamsCount;
    updateApplyingParamsLabel();

    QString error = status == 0 
        ? tr("Possibly, appropriate camera's service is unavailable now")
        : tr("Server returned the following error code : ") + status; 

   /* QString failedParams;
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
    }*/
}

void QnCameraAdvancedSettingsWidget::at_advancedParamChanged(const QString &id, const QString &value) {
    auto serverConnection = getServerConnection();
    if (!serverConnection)
        return;

	QnCameraAdvancedParamValue changedValue(id, value);
    serverConnection->setParamsAsync(m_camera, QnCameraAdvancedParamValueList() << changedValue, this, SLOT(at_advancedParam_saved(int, const QnCameraAdvancedParamValueList &, int)));
    ++m_applyingParamsCount;
    updateApplyingParamsLabel();
}
