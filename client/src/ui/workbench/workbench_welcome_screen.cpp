
#include "workbench_welcome_screen.h"

#include <QtQuick/QQuickView>
#include <QtQml/QQmlContext>

#include <common/common_module.h>

#include <watchers/cloud_status_watcher.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/models/systems_model.h>
#include <ui/models/last_system_users_model.h>
#include <ui/workbench/workbench_context.h>
#include <ui/style/nx_style.h>
#include <ui/dialogs/login_dialog.h>
#include <ui/dialogs/non_modal_dialog_constructor.h>

namespace
{
    typedef QPointer<QnWorkbenchWelcomeScreen> GuardType;

    QQuickView *createMainView(QObject *context)
    {
        static const auto kWelcomeScreenSource = lit("qrc:/src/qml/WelcomeScreen.qml");
        static const auto kContextVariableName = lit("context");

        qmlRegisterType<QnSystemsModel>("NetworkOptix.Qml", 1, 0, "QnSystemsModel");
        qmlRegisterType<QnLastSystemUsersModel>("NetworkOptix.Qml", 1, 0, "QnLastSystemConnectionsData");

        const auto quickView = new QQuickView();
        quickView->rootContext()->setContextProperty(
            kContextVariableName, context);
        quickView->setSource(kWelcomeScreenSource);

        return quickView;
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

    , m_cloudWatcher(qnCommon->instance<QnCloudStatusWatcher>())
    , m_palette(extractPalette())
    , m_widget(QWidget::createWindowContainer(createMainView(this)))
    , m_loginDialog()
    , m_visible(false)
    , m_enabled(false)
{
    NX_CRITICAL(m_cloudWatcher, Q_FUNC_INFO, "Cloud watcher does not exist");
    connect(m_cloudWatcher, &QnCloudStatusWatcher::loginChanged
        , this, &QnWorkbenchWelcomeScreen::cloudUserNameChanged);
    connect(m_cloudWatcher, &QnCloudStatusWatcher::statusChanged
        , this, &QnWorkbenchWelcomeScreen::isLoggedInToCloudChanged);

    //

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

        if (isVisible())
            enableScreen();
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

bool QnWorkbenchWelcomeScreen::isEnabled() const
{
    return m_enabled;
}

void QnWorkbenchWelcomeScreen::setEnabled(bool isEnabled)
{
    if (m_enabled == isEnabled)
        return;

    m_enabled = isEnabled;
    emit enabledChanged();
}

QString QnWorkbenchWelcomeScreen::cloudUserName() const
{
    return m_cloudWatcher->cloudLogin();
}

bool QnWorkbenchWelcomeScreen::isLoggedInToCloud() const
{
    return (m_cloudWatcher->status() == QnCloudStatusWatcher::Online);
}

void QnWorkbenchWelcomeScreen::connectToLocalSystem(const QString &serverUrl
    , const QString &userName
    , const QString &password)
{
    QUrl url = QUrl::fromUserInput(serverUrl);
    url.setScheme(lit("http"));
    if (!password.isEmpty())
        url.setPassword(password);
    if (!userName.isEmpty())
        url.setUserName(userName);

    menu()->trigger(QnActions::ConnectAction
        , QnActionParameters().withArgument(Qn::UrlRole, url));
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
    bool wasEmpty = (m_loginDialog == nullptr);
    QnNonModalDialogConstructor<QnLoginDialog> dialogConstructor(
        m_loginDialog, m_widget.data());

    if (wasEmpty)
    {
        connect(m_loginDialog, &QDialog::finished
            , this, &QnWorkbenchWelcomeScreen::enableScreen);
    }

    dialogConstructor.resetGeometry();
    setEnabled(false);
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
//

void QnWorkbenchWelcomeScreen::enableScreen()
{
    setEnabled(true);
}

void QnWorkbenchWelcomeScreen::showScreen()
{
    setVisible(true);
}
