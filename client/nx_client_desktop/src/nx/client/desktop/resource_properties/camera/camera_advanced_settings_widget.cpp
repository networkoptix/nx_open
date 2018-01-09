#include "camera_advanced_settings_widget.h"
#include "ui_camera_advanced_settings_widget.h"

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

#include <ui/common/aligner.h>
#include <ui/style/webview_style.h>

#include <vms_gateway_embeddable.h>

#include <nx/client/desktop/ui/common/clipboard_button.h>

#include "camera_advanced_settings_web_page.h"

namespace {

bool isStatusValid(Qn::ResourceStatus status)
{
    return status == Qn::Online || status == Qn::Recording;
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

using namespace ui;

CameraAdvancedSettingsWidget::CameraAdvancedSettingsWidget(QWidget* parent /* = 0*/):
    base_type(parent),
    ui(new Ui::CameraAdvancedSettingsWidget),
    m_page(Page::Empty)
{
    ui->setupUi(this);

    ui->cameraIdInputField->setReadOnly(true);

 /* ui->primaryStreamUrlInputField->setTitle() is called from updateFromResource() */
    ui->primaryStreamUrlInputField->setReadOnly(true);

    ui->secondaryStreamUrlInputField->setTitle(tr("Secondary Stream"));
    ui->secondaryStreamUrlInputField->setReadOnly(true);

    auto cameraIdLineEdit = ui->cameraIdInputField->findChild<QLineEdit*>();
    auto primaryLineEdit = ui->primaryStreamUrlInputField->findChild<QLineEdit*>();
    auto secondaryLineEdit = ui->secondaryStreamUrlInputField->findChild<QLineEdit*>();
    NX_ASSERT(cameraIdLineEdit && primaryLineEdit && secondaryLineEdit);

    ClipboardButton::createInline(cameraIdLineEdit, ClipboardButton::StandardType::copy);
    ClipboardButton::createInline(primaryLineEdit, ClipboardButton::StandardType::copy);
    ClipboardButton::createInline(secondaryLineEdit, ClipboardButton::StandardType::copy);

    QnAligner* aligner = new QnAligner(this);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
    aligner->addWidgets({
        ui->cameraIdInputField,
        ui->primaryStreamUrlInputField,
        ui->secondaryStreamUrlInputField });

    initWebView();

    connect(ui->cameraAdvancedParamsWidget,
        &CameraAdvancedParamsWidget::hasChangesChanged,
        this,
        &CameraAdvancedSettingsWidget::hasChangesChanged);
}

CameraAdvancedSettingsWidget::~CameraAdvancedSettingsWidget()
{
}

QnVirtualCameraResourcePtr CameraAdvancedSettingsWidget::camera() const
{
    return m_camera;
}

void CameraAdvancedSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
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
            &CameraAdvancedSettingsWidget::updatePage);
    }

    m_cameraAdvancedSettingsWebPage->setCamera(m_camera);
    ui->cameraAdvancedParamsWidget->setCamera(m_camera);
}

void CameraAdvancedSettingsWidget::updateFromResource()
{
    updatePage();
    updateUrls();

    bool isIoModule = m_camera && m_camera->isIOModule();

    ui->cameraIdInputField->setTitle(isIoModule
        ? tr("I/O Module Id")
        : tr("Camera Id"));

    ui->noSettingsLabel->setText(isIoModule
        ? tr("This I/O module has no advanced settings")
        : tr("This camera has no advanced settings"));

    ui->primaryStreamUrlInputField->setTitle(isIoModule
        ? tr("Audio Stream")
        : tr("Primary Stream"));

    ui->secondaryStreamUrlInputField->setHidden(isIoModule);

    QString urlPlaceholder = isIoModule
        ? tr("URL is not available. Open stream and try again.")
        : tr("URL is not available. Open video stream and try again.");

    ui->primaryStreamUrlInputField->setPlaceholderText(urlPlaceholder);
    ui->secondaryStreamUrlInputField->setPlaceholderText(urlPlaceholder);
}

void CameraAdvancedSettingsWidget::setPage(Page page)
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
            case Page::Unavailable:
                return ui->unavailablePage;
        }
        return nullptr;
    };

    ui->stackedWidget->setCurrentWidget(widgetByPage());
}


void CameraAdvancedSettingsWidget::updatePage()
{
    auto calculatePage =
        [this]
        {
            if (!m_camera || !isStatusValid(m_camera->getStatus()))
                return Page::Unavailable;

        QnResourceData resourceData = qnStaticCommon->dataPool()->data(m_camera);
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

void CameraAdvancedSettingsWidget::updateUrls()
{
    if (!m_camera)
    {
        ui->cameraIdInputField->setText(QString());
        ui->primaryStreamUrlInputField->setText(QString());
        ui->secondaryStreamUrlInputField->setText(QString());
    }
    else
    {
        ui->cameraIdInputField->setText(m_camera->getId().toSimpleString());

        bool isIoModule = m_camera->isIOModule();
        bool hasPrimaryStream = !isIoModule || m_camera->isAudioSupported();
        ui->primaryStreamUrlInputField->setEnabled(hasPrimaryStream);
        ui->primaryStreamUrlInputField->setText(hasPrimaryStream
            ? m_camera->sourceUrl(Qn::CR_LiveVideo)
            : tr("I/O module has no audio stream"));

        bool hasSecondaryStream = m_camera->hasDualStreaming2();
        ui->secondaryStreamUrlInputField->setEnabled(hasSecondaryStream);
        ui->secondaryStreamUrlInputField->setText(hasSecondaryStream
            ? m_camera->sourceUrl(Qn::CR_SecondaryLiveVideo)
            : tr("Camera has no secondary stream"));
    }
}

void CameraAdvancedSettingsWidget::reloadData()
{
    updatePage();

    if (m_page == Page::Web)
    {
        NX_ASSERT(m_camera);
        if (!m_camera)
            return;

        QnResourceData resourceData = qnStaticCommon->dataPool()->data(m_camera);
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
    else if (m_page == Page::Manual)
    {
        ui->cameraAdvancedParamsWidget->loadValues();
    }
}

bool CameraAdvancedSettingsWidget::hasChanges() const
{
    if (ui->stackedWidget->currentWidget() != ui->manualPage)
        return false;

    return ui->cameraAdvancedParamsWidget->hasChanges();
}

void CameraAdvancedSettingsWidget::submitToResource()
{
    if (!hasChanges())
        return;

    ui->cameraAdvancedParamsWidget->saveValues();
}

void CameraAdvancedSettingsWidget::hideEvent(QHideEvent *event)
{
    Q_UNUSED(event);
    if (m_page != Page::Web)
        return;

    ui->webView->triggerPageAction(QWebPage::Stop);
    ui->webView->triggerPageAction(QWebPage::StopScheduledPageRefresh);
    ui->webView->setContent(tr("Loading...").toUtf8());
}

bool CameraAdvancedSettingsWidget::eventFilter(QObject* watched, QEvent* event)
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

void CameraAdvancedSettingsWidget::initWebView()
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
        this, &CameraAdvancedSettingsWidget::at_authenticationRequired, Qt::DirectConnection);
    connect(ui->webView->page()->networkAccessManager(), &QNetworkAccessManager::proxyAuthenticationRequired,
        this, &CameraAdvancedSettingsWidget::at_proxyAuthenticationRequired, Qt::DirectConnection);
}

void CameraAdvancedSettingsWidget::at_authenticationRequired(QNetworkReply* reply, QAuthenticator* authenticator)
{
    Q_UNUSED(reply);

    QnMutexLocker locker(&m_cameraMutex);
    if (!m_camera)
        return;

    QAuthenticator auth = m_camera->getAuth();

    authenticator->setUser(auth.user());
    authenticator->setPassword(auth.password());
}

void CameraAdvancedSettingsWidget::at_proxyAuthenticationRequired(const QNetworkProxy& proxy, QAuthenticator* authenticator)
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

} // namespace desktop
} // namespace client
} // namespace nx
