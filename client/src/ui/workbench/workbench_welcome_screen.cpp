
#include "workbench_welcome_screen.h"

#include <QtQuickWidgets/QQuickWidget>
#include <QtQuick/QQuickView>
#include <QtQml/QQmlContext>

#include <common/common_module.h>

#include <watchers/cloud_status_watcher.h>

#include <utils/common/delayed.h>
#include <nx/utils/raii_guard.h>
#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/models/systems_model.h>
#include <ui/models/system_hosts_model.h>
#include <ui/models/last_system_users_model.h>
#include <ui/workbench/workbench_context.h>
#include <ui/style/nx_style.h>
#include <ui/dialogs/login_dialog.h>
#include <ui/dialogs/non_modal_dialog_constructor.h>
#include <ui/dialogs/setup_wizard_dialog.h>

namespace
{
    typedef QPointer<QnWorkbenchWelcomeScreen> GuardType;

    QWidget *createMainView(QObject *context)
    {
        static const auto kWelcomeScreenSource = lit("qrc:/src/qml/WelcomeScreen.qml");
        static const auto kContextVariableName = lit("context");

        qmlRegisterType<QnSystemsModel>("NetworkOptix.Qml", 1, 0, "QnSystemsModel");
        qmlRegisterType<QnSystemHostsModel>("NetworkOptix.Qml", 1, 0, "QnSystemHostsModel");
        qmlRegisterType<QnLastSystemUsersModel>("NetworkOptix.Qml", 1, 0, "QnLastSystemConnectionsData");

        const auto quickWidget = new QQuickWidget();
        quickWidget->rootContext()->setContextProperty(
            kContextVariableName, context);
        quickWidget->setSource(kWelcomeScreenSource);

        // Welcome screen holders prevents blinking of QML widget.
        // Moreover, QQuickWidget can't be placed as direct child of
        // other widget - thus we use this "proxy" holder + layout
        QWidget *welcomeScreenHolder = new QWidget();
        QHBoxLayout *layout = new QHBoxLayout(welcomeScreenHolder);
        layout->addWidget(quickWidget);
        return welcomeScreenHolder;
    }

    QnGenericPalette extractPalette()
    {
        const auto proxy = dynamic_cast<QProxyStyle *>(qApp->style());
        NX_ASSERT(proxy, Q_FUNC_INFO, "Invalid application style");
        const auto style = dynamic_cast<QnNxStyle *>(proxy ? proxy->baseStyle() : nullptr);
        NX_ASSERT(style, Q_FUNC_INFO, "Style of application is not NX");
        return (style ? style->genericPalette() : QnGenericPalette());
    }
}

QnWorkbenchWelcomeScreen::QnWorkbenchWelcomeScreen(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_visibleControls(true)
    , m_visible(false)
    , m_connectingNow(false)
    , m_cloudWatcher(qnCommon->instance<QnCloudStatusWatcher>())
    , m_palette(extractPalette())
    , m_widget(createMainView(this))
    , m_pageSize(m_widget->size())
{
    NX_CRITICAL(m_cloudWatcher, Q_FUNC_INFO, "Cloud watcher does not exist");
    connect(m_cloudWatcher, &QnCloudStatusWatcher::loginChanged
        , this, &QnWorkbenchWelcomeScreen::cloudUserNameChanged);
    connect(m_cloudWatcher, &QnCloudStatusWatcher::statusChanged
        , this, &QnWorkbenchWelcomeScreen::isLoggedInToCloudChanged);

    //
    m_widget->installEventFilter(this);
    const auto idChangedHandler = [this](const QnUuid & /* id */)
    {
        // We could be just reconnecting if remoteGuid is null.
        // So, just hide screen when it is not
        if (!qnCommon->remoteGUID().isNull())
            setVisible(false);
    };

    connect(qnCommon, &QnCommonModule::remoteIdChanged
        , this, idChangedHandler);
    connect(action(QnActions::DisconnectAction), &QAction::triggered
        , this, &QnWorkbenchWelcomeScreen::showScreen);

    connect(this, &QnWorkbenchWelcomeScreen::visibleChanged, this, [this]()
    {
        context()->action(QnActions::EscapeHotkeyAction)->setEnabled(!m_visible);
    });

    setVisible(true);
}

QnWorkbenchWelcomeScreen::~QnWorkbenchWelcomeScreen()
{}

QWidget *QnWorkbenchWelcomeScreen::widget()
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
    return m_cloudWatcher->cloudLogin();
}

bool QnWorkbenchWelcomeScreen::isLoggedInToCloud() const
{
    return (m_cloudWatcher->status() == QnCloudStatusWatcher::Online);
}

QSize QnWorkbenchWelcomeScreen::pageSize() const
{
    return m_pageSize;
}

void QnWorkbenchWelcomeScreen::setPageSize(const QSize &size)
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

bool QnWorkbenchWelcomeScreen::connectingNow() const
{
    return m_connectingNow;
}

void QnWorkbenchWelcomeScreen::setConnectingNow(bool value)
{
    if (m_connectingNow == value)
        return;

    m_connectingNow = value;
    emit connectingNowChanged();
}

void QnWorkbenchWelcomeScreen::connectToLocalSystem(const QString &serverUrl
    , const QString &userName
    , const QString &password)
{
    // TODO: #ynikitenkov add look after connection process
    // and don't allow to connect to two or more servers simultaneously
    const auto connectFunction = [this
        , serverUrl, userName, password]()
    {
        setConnectingNow(true);
        const auto controlsGuard = QnRaiiGuard::createDestructable(
            [this]() { setConnectingNow(false); });

        QUrl url = QUrl::fromUserInput(serverUrl);
        url.setScheme(lit("http"));
        if (!password.isEmpty())
            url.setPassword(password);
        if (!userName.isEmpty())
            url.setUserName(userName);

        menu()->trigger(QnActions::ConnectAction
            , QnActionParameters().withArgument(Qn::UrlRole, url));
    };

    enum { kMinimalDelay = 1};
    // We have to use delayed execution to prevent client crash 
    // when closing server that we are connecting to.
    executeDelayedParented(connectFunction, kMinimalDelay, this);
}

void QnWorkbenchWelcomeScreen::connectToCloudSystem(const QString &serverUrl)
{
    if (!isLoggedInToCloud())
        return;

    connectToLocalSystem(serverUrl, m_cloudWatcher->cloudLogin()
        , m_cloudWatcher->cloudPassword());
}

void QnWorkbenchWelcomeScreen::connectToAnotherSystem()
{
    const QScopedPointer<QnLoginDialog> dialog(
        new QnLoginDialog(context()->mainWindow()));

    dialog->exec();
}

void QnWorkbenchWelcomeScreen::setupFactorySystem(const QString &serverUrl)
{
    setVisibleControls(false);
    const auto controlsGuard = QnRaiiGuard::createDestructable(
        [this]() { setVisibleControls(true); });

    const auto showDialogHandler = [this, serverUrl, controlsGuard]()
    {
        /* We are receiving string with port but without protocol, so we must parse it. */
        const QScopedPointer<QnSetupWizardDialog> dialog(new QnSetupWizardDialog(mainWindow()));
        dialog->setUrl(QUrl::fromUserInput(serverUrl));
        if (isLoggedInToCloud())
        {
            dialog->setCloudLogin(m_cloudWatcher->cloudLogin());
            dialog->setCloudPassword(m_cloudWatcher->cloudPassword());
        }

        if (dialog->exec() != QDialog::Accepted)
            return;

        if (!dialog->localLogin().isEmpty() && !dialog->localPassword().isEmpty())
            connectToLocalSystem(serverUrl, dialog->localLogin(), dialog->localPassword());
        else if (!dialog->cloudLogin().isEmpty() && !dialog->cloudPassword().isEmpty())
            connectToLocalSystem(serverUrl, dialog->cloudLogin(), dialog->cloudPassword());
    };

    // Use delayed handling for proper animation
    enum { kNextEventDelay = 100 };
    executeDelayedParented(showDialogHandler
        , kNextEventDelay, this);
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
    menu()->trigger(QnActions::LoginToCLoud);
}

void QnWorkbenchWelcomeScreen::createAccount()
{
    menu()->trigger(QnActions::OpenCloudRegisterUrl);
}

void QnWorkbenchWelcomeScreen::tryHideScreen()
{
    if (!qnCommon->remoteGUID().isNull())
        setVisible(false);
}

//

QColor QnWorkbenchWelcomeScreen::getPaletteColor(const QString &group
    , int index)
{
    return m_palette.color(group, index);
}

QColor QnWorkbenchWelcomeScreen::getDarkerColor(const QColor &color
    , int offset)
{
    const auto paletteColor = m_palette.color(color);
    return paletteColor.darker(offset);
}

QColor QnWorkbenchWelcomeScreen::getLighterColor(const QColor &color
    , int offset)
{
    const auto paletteColor = m_palette.color(color);
    return paletteColor.lighter(offset);
}

QColor QnWorkbenchWelcomeScreen::colorWithAlpha(QColor color
    , qreal alpha)
{
    color.setAlphaF(alpha);
    return color;
}

void QnWorkbenchWelcomeScreen::showScreen()
{
    setVisible(true);
}

bool QnWorkbenchWelcomeScreen::eventFilter(QObject *obj
    , QEvent *event)
{
    switch(event->type())
    {
    case QEvent::Resize:
        if (auto resizeEvent = dynamic_cast<QResizeEvent *>(event))
            setPageSize(resizeEvent->size());
        break;
    }


    return base_type::eventFilter(obj, event);
}
