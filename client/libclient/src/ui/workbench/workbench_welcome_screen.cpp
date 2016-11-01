
#include "workbench_welcome_screen.h"

#include <QtQuickWidgets/QQuickWidget>
#include <QtQuick/QQuickView>
#include <QtQml/QQmlContext>

#include <common/common_module.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/resource.h>

#include <watchers/cloud_status_watcher.h>
#include <utils/common/delayed.h>
#include <utils/common/app_info.h>
#include <utils/connection_diagnostics_helper.h>
#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/models/system_hosts_model.h>
#include <ui/models/filtering_systems_model.h>
#include <ui/models/recent_local_connections_model.h>
#include <ui/workbench/workbench_context.h>
#include <ui/style/nx_style.h>
#include <ui/dialogs/login_dialog.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>
#include <ui/dialogs/setup_wizard_dialog.h>
#include <ui/workbench/workbench_resource.h>
#include <client/forgotten_systems_manager.h>
#include <client_core/client_core_settings.h>

#include <utils/common/app_info.h>

namespace
{
    typedef QPointer<QnWorkbenchWelcomeScreen> GuardType;

    QWidget* createMainView(QObject* context, QQuickView* quickView)
    {
        static const auto kWelcomeScreenSource = lit("qrc:/src/qml/WelcomeScreen.qml");
        static const auto kContextVariableName = lit("context");

        qmlRegisterType<QnSystemHostsModel>("NetworkOptix.Qml", 1, 0, "QnSystemHostsModel");
        qmlRegisterType<QnRecentLocalConnectionsModel>("NetworkOptix.Qml", 1, 0, "QnRecentLocalConnectionsModel");
        qmlRegisterType<QnFilteringSystemsModel>("NetworkOptix.Qml", 1, 0, "QnFilteringSystemsModel");

        auto holder = new QStackedWidget();
        holder->addWidget(new QWidget());
        holder->addWidget(QWidget::createWindowContainer(quickView));

        const auto loadQmlData = [quickView, context, holder]()
            {
                const auto updateQmlViewVisibility = [holder](QQuickView::Status status)
                    {
                        if (status != QQuickView::Ready)
                            return false;

                        enum { kQmlViewIndex = 1 };
                        holder->setCurrentIndex(kQmlViewIndex);
                        return true;
                    };

                QObject::connect(quickView, &QQuickView::statusChanged,
                    quickView, updateQmlViewVisibility);

                quickView->rootContext()->setContextProperty(
                    kContextVariableName, context);
                quickView->setSource(kWelcomeScreenSource);
            };

        // Async load of qml data
        executeDelayedParented(loadQmlData, 0, quickView);
        return holder;
    }

    QnGenericPalette extractPalette()
    {
        const auto proxy = dynamic_cast<QProxyStyle *>(qApp->style());
        NX_ASSERT(proxy, Q_FUNC_INFO, "Invalid application style");
        const auto style = dynamic_cast<QnNxStyle *>(proxy ? proxy->baseStyle() : nullptr);
        NX_ASSERT(style, Q_FUNC_INFO, "Style of application is not NX");
        return (style ? style->genericPalette() : QnGenericPalette());
    }


    QnResourceList extractResources(const UrlsList& urls)
    {
        QMimeData data;
        data.setUrls(urls);
        return QnWorkbenchResource::deserializeResources(&data);
    }
}

QnWorkbenchWelcomeScreen::QnWorkbenchWelcomeScreen(QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),

    m_receivingResources(false),
    m_visibleControls(true),
    m_visible(false),
    m_connectingSystemName(),
    m_palette(extractPalette()),
    m_quickView(new QQuickView()),
    m_widget(createMainView(this, m_quickView)),
    m_pageSize(m_widget->size()),
    m_message(),
    m_appInfo(new QnAppInfo(this))
{
    NX_CRITICAL(qnCloudStatusWatcher, Q_FUNC_INFO, "Cloud watcher does not exist");
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::loginChanged,
        this, &QnWorkbenchWelcomeScreen::cloudUserNameChanged);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged,
        this, &QnWorkbenchWelcomeScreen::isLoggedInToCloudChanged);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::isCloudEnabledChanged,
        this, &QnWorkbenchWelcomeScreen::isCloudEnabledChanged);

    //
    m_widget->installEventFilter(this);
    m_quickView->installEventFilter(this);
    qApp->installEventFilter(this); //< QTBUG-34414 workaround
    connect(action(QnActions::DisconnectAction), &QAction::triggered,
        this, &QnWorkbenchWelcomeScreen::showScreen);

    connect(this, &QnWorkbenchWelcomeScreen::visibleChanged, this, [this]()
    {
        if (!m_visible)
            setGlobalPreloaderVisible(false);   ///< Auto toggle off preloader

        context()->action(QnActions::EscapeHotkeyAction)->setEnabled(!m_visible);
    });

    setVisible(true);

    connect(qnSettings, &QnClientSettings::valueChanged, this, [this](int valueId)
    {
        if (valueId == QnClientSettings::AUTO_LOGIN)
            emit resetAutoLogin();
    });
}

QnWorkbenchWelcomeScreen::~QnWorkbenchWelcomeScreen()
{}

QWidget* QnWorkbenchWelcomeScreen::widget()
{
    return m_widget.data();
}

bool QnWorkbenchWelcomeScreen::isVisible() const
{
    return m_visible;
}

void QnWorkbenchWelcomeScreen::setVisible(bool isVisible)
{
    if (m_visible == isVisible)
        return;

    m_visible = isVisible;

    emit visibleChanged();
}

QString QnWorkbenchWelcomeScreen::cloudUserName() const
{
    return qnCloudStatusWatcher->cloudLogin();
}

bool QnWorkbenchWelcomeScreen::isLoggedInToCloud() const
{
    return (qnCloudStatusWatcher->status() != QnCloudStatusWatcher::LoggedOut);
}

bool QnWorkbenchWelcomeScreen::isCloudEnabled() const
{
    return qnCloudStatusWatcher->isCloudEnabled();
}

QSize QnWorkbenchWelcomeScreen::pageSize() const
{
    return m_pageSize;
}

void QnWorkbenchWelcomeScreen::setPageSize(const QSize& size)
{
    if (m_pageSize == size)
        return;

    m_pageSize = size;
    emit pageSizeChanged();
}

bool QnWorkbenchWelcomeScreen::visibleControls() const
{
    return m_visibleControls;
}

void QnWorkbenchWelcomeScreen::setVisibleControls(bool visible)
{
    if (m_visibleControls == visible)
        return;

    m_visibleControls = visible;
    emit visibleControlsChanged();
}

QString QnWorkbenchWelcomeScreen::connectingToSystem() const
{
    return m_connectingSystemName;
}

void QnWorkbenchWelcomeScreen::openConnectingTile()
{
    const auto systemId = connectingToSystem();
    if (systemId.isEmpty())
        return;

    executeDelayedParented([this, systemId]() { emit openTile(systemId);  }, 0, this);
}
void QnWorkbenchWelcomeScreen::handleDisconnectedFromSystem()
{
    const auto systemId = connectingToSystem();
    if (systemId.isEmpty())
        return;

    setConnectingToSystem(QString());
}

void QnWorkbenchWelcomeScreen::handleConnectingToSystem()
{
    setConnectingToSystem(QString());
}

void QnWorkbenchWelcomeScreen::setConnectingToSystem(const QString& value)
{
    if (m_connectingSystemName == value)
        return;

    m_connectingSystemName = value;
    emit connectingToSystemChanged();
}

bool QnWorkbenchWelcomeScreen::globalPreloaderVisible() const
{
    return m_receivingResources;
}

void QnWorkbenchWelcomeScreen::setGlobalPreloaderVisible(bool value)
{
    if (value == m_receivingResources)
        return;

    m_receivingResources = value;
    emit globalPreloaderVisibleChanged();
}

QString QnWorkbenchWelcomeScreen::minSupportedVersion() const
{
    return QnConnectionValidator::minSupportedVersion().toString();
}

void QnWorkbenchWelcomeScreen::setMessage(const QString& message)
{
    if (m_message == message)
        return;

    m_message = message;

    emit messageChanged();

    m_widget->repaint();
    m_widget->window()->repaint();
    m_widget->window()->update();

    qApp->flush();
    qApp->sendPostedEvents();
}

QString QnWorkbenchWelcomeScreen::message() const
{
    return m_message;
}

QnAppInfo* QnWorkbenchWelcomeScreen::appInfo() const
{
    return m_appInfo;
}

bool QnWorkbenchWelcomeScreen::isAcceptableDrag(const QList<QUrl>& urls)
{
    return !extractResources(urls).isEmpty();
}

void QnWorkbenchWelcomeScreen::makeDrop(const QList<QUrl>& urls)
{
    const auto resources = extractResources(urls);
    if (resources.isEmpty())
        return;

    menu()->triggerIfPossible(QnActions::DropResourcesAction, QnActionParameters(resources));
}

void QnWorkbenchWelcomeScreen::connectToLocalSystem(
    const QString& systemId,
    const QString& serverUrl,
    const QString& userName,
    const QString& password,
    bool storePassword,
    bool autoLogin)
{
    connectToSystemInternal(
        systemId,
        QUrl::fromUserInput(serverUrl),
        QnCredentials(userName, password),
        storePassword,
        autoLogin);
}

void QnWorkbenchWelcomeScreen::forceActiveFocus()
{
    m_quickView->requestActivate();
}

void QnWorkbenchWelcomeScreen::connectToSystemInternal(
    const QString& systemId,
    const QUrl& serverUrl,
    const QnCredentials& credentials,
    bool storePassword,
    bool autoLogin,
    const QnRaiiGuardPtr& completionTracker)
{
    if (!connectingToSystem().isEmpty())
        return; //< Connection process is in progress

    // TODO: #ynikitenkov add look after connection process
    // and don't allow to connect to two or more servers simultaneously
    const auto connectFunction =
        [this, serverUrl, credentials, storePassword, autoLogin, systemId, completionTracker]()
        {
            setConnectingToSystem(systemId);

            QUrl url = serverUrl;
            if (!credentials.password.isEmpty())
                url.setPassword(credentials.password);
            if (!credentials.user.isEmpty())
                url.setUserName(credentials.user);

            QnActionParameters params;
            params.setArgument(Qn::UrlRole, url);
            params.setArgument(Qn::StorePasswordRole, storePassword);
            params.setArgument(Qn::AutoLoginRole, autoLogin);
            menu()->trigger(QnActions::ConnectAction, params);
        };

    enum { kMinimalDelay = 1};
    // We have to use delayed execution to prevent client crash
    // when closing server that we are connecting to.
    executeDelayedParented(connectFunction, kMinimalDelay, this);
}

void QnWorkbenchWelcomeScreen::connectToCloudSystem(const QString& systemId, const QString& serverUrl)
{
    if (!isLoggedInToCloud())
        return;

    connectToSystemInternal(systemId, QUrl(serverUrl),
        qnCloudStatusWatcher->credentials(), false, false);
}

void QnWorkbenchWelcomeScreen::connectToAnotherSystem()
{
    menu()->trigger(QnActions::OpenLoginDialogAction);
}

void QnWorkbenchWelcomeScreen::setupFactorySystem(const QString& serverUrl)
{
    setVisibleControls(false);
    const auto controlsGuard = QnRaiiGuard::createDestructable(
        [this]() { setVisibleControls(true); });

    const auto showDialogHandler = [this, serverUrl, controlsGuard]()
        {
            /* We are receiving string with port but without protocol, so we must parse it. */
            const QScopedPointer<QnSetupWizardDialog> dialog(new QnSetupWizardDialog(mainWindow()));

            dialog->setUrl(QUrl(serverUrl));
            if (isLoggedInToCloud())
                dialog->setCloudCredentials(qnCloudStatusWatcher->credentials());

            if (dialog->exec() != QDialog::Accepted)
                return;

            if (dialog->localCredentials().isValid())
            {
                connectToSystemInternal(QString(), serverUrl, dialog->localCredentials(),
                    false, false, controlsGuard);
            }
            else if (dialog->cloudCredentials().isValid())
            {
                const auto cloudCredentials = dialog->cloudCredentials();

                // We suppose that connection to cloud from new systems is always permanent
                qnClientCoreSettings->setCloudLogin(cloudCredentials.user);
                qnClientCoreSettings->setCloudPassword(cloudCredentials.password);

                qnCloudStatusWatcher->setCloudCredentials(cloudCredentials, true);
                connectToSystemInternal(QString(), serverUrl, cloudCredentials,
                    false, false, controlsGuard);
            }

        };

    // Use delayed handling for proper animation
    static const int kNextEventDelayMs = 100;
    executeDelayedParented(showDialogHandler, kNextEventDelayMs, this);
}

void QnWorkbenchWelcomeScreen::logoutFromCloud()
{
    menu()->trigger(QnActions::LogoutFromCloud);
}

void QnWorkbenchWelcomeScreen::manageCloudAccount()
{
    menu()->trigger(QnActions::OpenCloudManagementUrl);
}

void QnWorkbenchWelcomeScreen::loginToCloud()
{
    menu()->trigger(QnActions::LoginToCloud);
}

void QnWorkbenchWelcomeScreen::createAccount()
{
    menu()->trigger(QnActions::OpenCloudRegisterUrl);
}

//

QColor QnWorkbenchWelcomeScreen::getPaletteColor(const QString& group, int index)
{
    return m_palette.color(group, index);
}

QColor QnWorkbenchWelcomeScreen::getDarkerColor(const QColor& color, int offset)
{
    const auto paletteColor = m_palette.color(color);
    return paletteColor.darker(offset);
}

QColor QnWorkbenchWelcomeScreen::getLighterColor(const QColor& color, int offset)
{
    const auto paletteColor = m_palette.color(color);
    return paletteColor.lighter(offset);
}

QColor QnWorkbenchWelcomeScreen::colorWithAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(alpha);
    return color;
}

void QnWorkbenchWelcomeScreen::hideSystem(const QString& systemId)
{
    qnForgottenSystemsManager->forgetSystem(systemId);
}

void QnWorkbenchWelcomeScreen::showScreen()
{
    setVisible(true);
}

bool QnWorkbenchWelcomeScreen::eventFilter(QObject* obj, QEvent* event)
{
    if (obj != m_widget)
        return base_type::eventFilter(obj, event);

    switch(event->type())
    {
        case QEvent::Resize:
            if (auto resizeEvent = dynamic_cast<QResizeEvent *>(event))
                setPageSize(resizeEvent->size());
            break;
#if defined(Q_OS_MACX)
        case QEvent::WindowActivate:
            m_widget->activateWindow(); //< QTBUG-34414 workaround
            break;
#endif
        default:
            break;
    }

    return base_type::eventFilter(obj, event);
}
