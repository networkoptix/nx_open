#include "workbench_welcome_screen.h"

#include <QtCore/QMimeData>

#include <QtQuickWidgets/QQuickWidget>
#include <QtQuick/QQuickItem>
#include <QtQml/QQmlContext>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QWhatsThis>

#include <client/startup_tile_manager.h>
#include <client/forgotten_systems_manager.h>
#include <client_core/client_core_settings.h>
#include <client/client_runtime_settings.h>

#include <common/common_module.h>

#include <core/resource/resource.h>
#include <core/resource/file_processor.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>
#include <ui/style/nx_style.h>
#include <ui/dialogs/login_dialog.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>
#include <ui/dialogs/setup_wizard_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <finders/systems_finder.h>
#include <helpers/system_helpers.h>
#include <watchers/cloud_status_watcher.h>

#include <utils/common/app_info.h>
#include <utils/common/util.h>
#include <utils/common/delayed.h>
#include <utils/common/app_info.h>
#include <utils/connection_diagnostics_helper.h>

#include <nx/utils/log/log.h>

#include <helpers/system_helpers.h>

using namespace nx::client::desktop::ui;

namespace {

QnResourceList extractResources(const QList<QUrl>& urls)
{
    return QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(urls));
}

// Extracts url from string and changes port to default if it is invalid
QUrl urlFromUserInput(const QString& value)
{
    auto result = QUrl::fromUserInput(value);
    if (result.port() <= 0)
        result.setPort(DEFAULT_APPSERVER_PORT);

    return result;
}

} // namespace

QnWorkbenchWelcomeScreen::QnWorkbenchWelcomeScreen(
    QQmlEngine* engine,
    QWidget* parent)
    :
    base_type(engine, parent),
    QnWorkbenchContextAware(parent)
{
    NX_EXPECT(qnRuntime->isDesktopMode());

    rootContext()->setContextProperty(lit("context"), this);
    setSource(lit("Nx/WelcomeScreen/WelcomeScreen.qml"));
    setResizeMode(QQuickWidget::SizeRootObjectToView);

    if (status() == QQuickWidget::Error)
    {
        for (const auto& error: errors())
            NX_ERROR(this, error.toString());
        NX_CRITICAL(false, Q_FUNC_INFO, "Welcome screen loading failed.");
    }

    NX_CRITICAL(qnStartupTileManager, Q_FUNC_INFO, "Startup tile manager does not exists");
    NX_CRITICAL(qnCloudStatusWatcher, Q_FUNC_INFO, "Cloud watcher does not exist");
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::loginChanged,
        this, &QnWorkbenchWelcomeScreen::cloudUserNameChanged);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged,
        this, &QnWorkbenchWelcomeScreen::isLoggedInToCloudChanged);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::isCloudEnabledChanged,
        this, &QnWorkbenchWelcomeScreen::isCloudEnabledChanged);

    setHelpTopic(this, Qn::Login_Help);

    connect(qnSettings, &QnClientSettings::valueChanged, this, [this](int valueId)
    {
        if (valueId == QnClientSettings::AUTO_LOGIN)
            emit resetAutoLogin();
    });

    connect(qnStartupTileManager, &QnStartupTileManager::tileActionRequested,
        this, &QnWorkbenchWelcomeScreen::handleStartupTileAction);
}

QnWorkbenchWelcomeScreen::~QnWorkbenchWelcomeScreen()
{
}

void QnWorkbenchWelcomeScreen::handleStartupTileAction(const QString& systemId, bool initial)
{
    const auto system = qnSystemsFinder->getSystem(systemId);
    NX_ASSERT(system, "System is empty");
    if (!system)
        return;

    if (qnSettings->autoLogin())
        return; // Do nothing in case of auto-login option set

    if (system->isCloudSystem() || !system->isConnectable())
        return; // Do nothing with cloud and not connectable systems

    static const auto wrongServers =
        [](const QnSystemDescriptionPtr& system) -> bool
        {
            const bool wrong = system->servers().isEmpty();
            NX_ASSERT(!wrong, "Wrong local system - at least one server should exist");
            return wrong;
        };

    if (system->isNewSystem())
    {
        if (!initial)
            return; // We found system after initial discovery

        if (wrongServers(system))
            return;

        const auto firstServerId = system->servers().first().id;
        const auto host = system->getServerHost(firstServerId);
        setupFactorySystem(host.toString());
        return;
    }

    // Here we have online local system.

    if (initial)
    {
        if (wrongServers(system))
            return;

        const auto credentialsList =
            qnClientCoreSettings->systemAuthenticationData()[system->localId()];

        if (!credentialsList.isEmpty() && !credentialsList.first().password.isEmpty())
        {
            static const bool kNeverAutologin = false;
            static const bool kAlwaysStorePassword = true;

            const auto credentials = credentialsList.first();
            const auto firstServerId = system->servers().first().id;
            const auto serverHost = system->getServerHost(firstServerId);

            connectToLocalSystem(system->id(), serverHost.toString(),
                credentials.user, credentials.password.value(),
                kAlwaysStorePassword, kNeverAutologin);

            return;
        }
    }

    // Just expand online local tile
    executeDelayedParented([this, system]() { emit openTile(system->id());  }, 0, this);
}

void QnWorkbenchWelcomeScreen::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);

    action(action::EscapeHotkeyAction)->setEnabled(false);
}

void QnWorkbenchWelcomeScreen::hideEvent(QHideEvent* event)
{
    base_type::hideEvent(event);

    setGlobalPreloaderVisible(false); //< Auto toggle off preloader
    qnStartupTileManager->skipTileAction(); //< available only on first show

    action(action::EscapeHotkeyAction)->setEnabled(true);
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

    // Repainting the widget to guarantee that started screen recording won't contain the visible
    // message.
    // TODO: Find a better way to achieve the same effect.
    repaint();
    window()->repaint();
    window()->update();

    qApp->flush();
    qApp->sendPostedEvents();
}

QString QnWorkbenchWelcomeScreen::message() const
{
    return m_message;
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

    if (menu()->triggerIfPossible(action::DropResourcesAction, resources))
        action(action::ResourcesModeAction)->setChecked(true);
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
        urlFromUserInput(serverUrl),
        QnEncodedCredentials(userName, password),
        storePassword,
        autoLogin);
}

void QnWorkbenchWelcomeScreen::forgetPassword(
    const QString& localSystemId,
    const QString& userName)
{
    const auto localId = QnUuid::fromStringSafe(localSystemId);
    if (localId.isNull())
        return;

    NX_DEBUG(nx::client::core::helpers::kCredentialsLogTag, lm("Forget password of %1 to the system %2")
        .arg(userName).arg(localSystemId));

    const auto callback = [localId, userName]()
        {
            nx::client::core::helpers::storeCredentials(
                localId, QnEncodedCredentials(userName, QString()));
        };

    executeDelayedParented(callback, 0, this);
}

void QnWorkbenchWelcomeScreen::forceActiveFocus()
{
    if (const auto rootItem = rootObject())
        rootItem->forceActiveFocus();
}

void QnWorkbenchWelcomeScreen::connectToSystemInternal(
    const QString& systemId,
    const QUrl& serverUrl,
    const QnEncodedCredentials& credentials,
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
                url.setPassword(credentials.password.value());
            if (!credentials.user.isEmpty())
                url.setUserName(credentials.user);

            action::Parameters params;
            params.setArgument(Qn::UrlRole, url);
            params.setArgument(Qn::StorePasswordRole, storePassword);
            params.setArgument(Qn::AutoLoginRole, autoLogin);
            params.setArgument(Qn::StoreSessionRole, true);
            menu()->trigger(action::ConnectAction, params);
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

    const bool autoLogin = qnCloudStatusWatcher->stayConnected();
    connectToSystemInternal(systemId, QUrl(serverUrl),
        qnCloudStatusWatcher->credentials(), false, autoLogin);
}

void QnWorkbenchWelcomeScreen::connectToAnotherSystem()
{
    menu()->trigger(action::OpenLoginDialogAction);
}

void QnWorkbenchWelcomeScreen::setupFactorySystem(const QString& serverUrl)
{
    setVisibleControls(false);
    const auto controlsGuard = QnRaiiGuard::createDestructible(
        [this]() { setVisibleControls(true); });

    const auto showDialogHandler = [this, serverUrl, controlsGuard]()
        {
            /* We are receiving string with port but without protocol, so we must parse it. */
            const QScopedPointer<QnSetupWizardDialog> dialog(new QnSetupWizardDialog(mainWindowWidget()));

            dialog->setUrl(QUrl(serverUrl));
            if (isLoggedInToCloud())
                dialog->setCloudCredentials(qnCloudStatusWatcher->credentials());

            if (dialog->exec() != QDialog::Accepted)
                return;

            static constexpr bool kNoAutoLogin = false;
            if (dialog->localCredentials().isValid())
            {
                connectToSystemInternal(QString(), serverUrl, dialog->localCredentials(),
                    dialog->savePassword(), kNoAutoLogin, controlsGuard);
            }
            else if (dialog->cloudCredentials().isValid())
            {
                const auto cloudCredentials = dialog->cloudCredentials();

                if (dialog->savePassword())
                {
                    qnClientCoreSettings->setCloudLogin(cloudCredentials.user);
                    qnClientCoreSettings->setCloudPassword(cloudCredentials.password.value());
                    qnClientCoreSettings->save();
                }

                qnCloudStatusWatcher->setCredentials(cloudCredentials, true);
                connectToSystemInternal(QString(), serverUrl, cloudCredentials,
                    dialog->savePassword(), kNoAutoLogin, controlsGuard);
            }

        };

    // Use delayed handling for proper animation
    static const int kNextEventDelayMs = 100;
    executeDelayedParented(showDialogHandler, kNextEventDelayMs, this);
}

void QnWorkbenchWelcomeScreen::logoutFromCloud()
{
    menu()->trigger(action::LogoutFromCloud);
}

void QnWorkbenchWelcomeScreen::manageCloudAccount()
{
    menu()->trigger(action::OpenCloudManagementUrl);
}

void QnWorkbenchWelcomeScreen::loginToCloud()
{
    menu()->trigger(action::LoginToCloud);
}

void QnWorkbenchWelcomeScreen::createAccount()
{
    menu()->trigger(action::OpenCloudRegisterUrl);
}

//

void QnWorkbenchWelcomeScreen::hideSystem(const QString& systemId, const QString& localSystemId)
{
    qnForgottenSystemsManager->forgetSystem(systemId);
    qnForgottenSystemsManager->forgetSystem(localSystemId);
}
