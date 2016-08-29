
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
#include <ui/models/systems_model.h>
#include <ui/models/system_hosts_model.h>
#include <ui/models/recent_user_connections_model.h>
#include <ui/models/qml_sort_filter_proxy_model.h>
#include <ui/workbench/workbench_context.h>
#include <ui/style/nx_style.h>
#include <ui/dialogs/login_dialog.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>
#include <ui/dialogs/setup_wizard_dialog.h>
#include <ui/workbench/workbench_resource.h>

namespace
{
    typedef QPointer<QnWorkbenchWelcomeScreen> GuardType;

    QWidget* createMainView(QObject* context)
    {
        static const auto kWelcomeScreenSource = lit("qrc:/src/qml/WelcomeScreen.qml");
        static const auto kContextVariableName = lit("context");

        qmlRegisterType<QnSystemsModel>("NetworkOptix.Qml", 1, 0, "QnSystemsModel");
        qmlRegisterType<QnSystemHostsModel>("NetworkOptix.Qml", 1, 0, "QnSystemHostsModel");
        qmlRegisterType<QnQmlSortFilterProxyModel>("NetworkOptix.Qml", 1, 0, "QnQmlSortFilterProxyModel");
        qmlRegisterType<QnRecentUserConnectionsModel>("NetworkOptix.Qml", 1, 0, "QnRecentUserConnectionsModel");

        const auto quickView = new QQuickView();
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
    m_widget(createMainView(this)),
    m_pageSize(m_widget->size())
{
    NX_CRITICAL(qnCloudStatusWatcher, Q_FUNC_INFO, "Cloud watcher does not exist");
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::loginChanged,
        this, &QnWorkbenchWelcomeScreen::cloudUserNameChanged);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged,
        this, &QnWorkbenchWelcomeScreen::isLoggedInToCloudChanged);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged,
        this, &QnWorkbenchWelcomeScreen::isOfflineConnectionChanged);

    //
    m_widget->installEventFilter(this);
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

bool QnWorkbenchWelcomeScreen::isOfflineConnection() const
{
    return (qnCloudStatusWatcher->status() == QnCloudStatusWatcher::Offline);
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

QString QnWorkbenchWelcomeScreen::softwareVersion() const
{
    return QnAppInfo::applicationVersion();
}

bool QnWorkbenchWelcomeScreen::isAcceptableDrag(const UrlsList& urls)
{
    return !extractResources(urls).isEmpty();
}

void QnWorkbenchWelcomeScreen::makeDrop(const UrlsList& urls)
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
    connectToLocalSystemImpl(systemId, serverUrl, userName, password, storePassword, autoLogin);
}

void QnWorkbenchWelcomeScreen::connectToLocalSystemImpl(
    const QString& systemId,
    const QString& serverUrl,
    const QString& userName,
    const QString& password,
    bool storePassword,
    bool autoLogin,
    const QnRaiiGuardPtr& completionTracker)
{
    if (!connectingToSystem().isEmpty())
        return; //< Connection process is in progress

    // TODO: #ynikitenkov add look after connection process
    // and don't allow to connect to two or more servers simultaneously
    const auto connectFunction =
        [this, serverUrl, userName, password, storePassword, autoLogin, systemId, completionTracker]()
        {
            setConnectingToSystem(systemId);

            const auto completionGuard = QnRaiiGuard::createDestructable(
                [this]() { setConnectingToSystem(QString()); });

            QUrl url = QUrl::fromUserInput(serverUrl);
            url.setScheme(lit("http"));
            if (!password.isEmpty())
                url.setPassword(password);
            if (!userName.isEmpty())
                url.setUserName(userName);

            QnActionParameters params;
            params.setArgument(Qn::UrlRole, url);
            params.setArgument(Qn::StorePasswordRole, storePassword);
            params.setArgument(Qn::ForceRemoveOldConnectionRole, !storePassword);
            params.setArgument(Qn::AutoLoginRole, autoLogin);
            params.setArgument(Qn::CompletionWatcherRole, completionGuard);

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

    connectToLocalSystem(systemId, serverUrl, qnCloudStatusWatcher->cloudLogin(),
        qnCloudStatusWatcher->cloudPassword(), false, false);
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

        QUrl targetUrl = QUrl::fromUserInput(serverUrl);
        targetUrl.setScheme(lit("http"));
        dialog->setUrl(targetUrl);
        if (isLoggedInToCloud())
        {
            dialog->setCloudLogin(qnCloudStatusWatcher->cloudLogin());
            dialog->setCloudPassword(qnCloudStatusWatcher->cloudPassword());
        }

        if (dialog->exec() != QDialog::Accepted)
            return;

        if (!dialog->localLogin().isEmpty() && !dialog->localPassword().isEmpty())
        {
            connectToLocalSystemImpl(QString(), serverUrl, dialog->localLogin(),
                dialog->localPassword(), false, false, controlsGuard);
        }
        else if (!dialog->cloudLogin().isEmpty() && !dialog->cloudPassword().isEmpty())
        {
            qnCommon->instance<QnCloudStatusWatcher>()->setCloudCredentials(dialog->cloudLogin(), dialog->cloudPassword(), true);
            connectToLocalSystemImpl(QString(), serverUrl, dialog->cloudLogin(),
                dialog->cloudPassword(), false, false, controlsGuard);
        }

    };

    // Use delayed handling for proper animation
    enum { kNextEventDelay = 100 };
    executeDelayedParented(showDialogHandler, kNextEventDelay, this);
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
