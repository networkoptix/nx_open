#include "workbench_welcome_screen.h"

#include <QtCore/QMimeData>

#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>
#include <QtQml/QQmlContext>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>

#include <client_core/client_core_settings.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <client_core/client_core_module.h>
#include <client/startup_tile_manager.h>
#include <client/forgotten_systems_manager.h>
#include <client/client_runtime_settings.h>
#include <client/system_weights_manager.h>

#include <common/common_module.h>

#include <core/resource/resource.h>
#include <core/resource/file_processor.h>

#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/common/utils/connection_url_parser.h>
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
#include <utils/common/credentials.h>
#include <utils/connection_diagnostics_helper.h>

#include <nx/utils/log/log.h>

#include <nx/vms/client/desktop/ini.h>

#include <helpers/system_helpers.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

namespace {

QnResourceList extractResources(const QList<QUrl>& urls)
{
    return QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(urls));
}

} // namespace

QnWorkbenchWelcomeScreen::QnWorkbenchWelcomeScreen(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_view(new QQuickView(qnClientCoreModule->mainQmlEngine(), nullptr))
{
    NX_ASSERT(qnRuntime->isDesktopMode());

    m_view->rootContext()->setContextProperty(lit("context"), this);
    m_view->setSource(lit("Nx/WelcomeScreen/WelcomeScreen.qml"));
    m_view->setResizeMode(QQuickView::SizeRootObjectToView);

    if (m_view->status() == QQuickView::Error)
    {
        for (const auto& error: m_view->errors())
            NX_ERROR(this, error.toString());
        NX_CRITICAL(false, "Welcome screen loading failed.");
    }

    const auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(QMargins());
    layout->addWidget(QWidget::createWindowContainer(m_view), 1);

    NX_CRITICAL(qnStartupTileManager, "Startup tile manager does not exists");
    NX_CRITICAL(qnCloudStatusWatcher, "Cloud watcher does not exist");
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
            nx::vms::client::core::settings()->systemAuthenticationData()[system->localId()];

        if (!credentialsList.isEmpty() && !credentialsList.first().password.isEmpty())
        {
            static const bool kNeverAutologin = false;
            static const bool kAlwaysStorePassword = true;

            const auto credentials = credentialsList.first();
            const auto firstServerId = system->servers().first().id;
            const auto serverHost = system->getServerHost(firstServerId);

            connectToLocalSystem(system->id(), serverHost.toString(),
                credentials.user, credentials.password,
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

void QnWorkbenchWelcomeScreen::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange)
        m_view->setColor(palette().color(QPalette::Window));

    base_type::changeEvent(event);
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
    m_view->update();
    repaint();

    qApp->flush();
    qApp->sendPostedEvents();
}

QString QnWorkbenchWelcomeScreen::message() const
{
    return m_message;
}

void QnWorkbenchWelcomeScreen::activateView() const
{
    return m_view->requestActivate();
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
        parseConnectionUrlFromUserInput(serverUrl),
        {userName, password},
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

    NX_DEBUG(nx::vms::client::core::helpers::kCredentialsLogTag, lm("Forget password of %1 to the system %2")
        .arg(userName).arg(localSystemId));

    const auto callback = [localId, userName]()
        {
            auto authData = nx::vms::client::core::settings()->systemAuthenticationData();
            auto& credentialsList = authData[localId];
            const auto it = std::find_if(credentialsList.begin(), credentialsList.end(),
                [userName](const auto& other)
                {
                    return other.user == userName;
                });

            if (it == credentialsList.end())
                return;

            const auto insertionIndex = it - credentialsList.begin();
            credentialsList.erase(it);
            credentialsList.insert(insertionIndex, {userName, QString()});

            nx::vms::client::core::settings()->systemAuthenticationData = authData;
        };

    executeDelayedParented(callback, 0, this);
}

void QnWorkbenchWelcomeScreen::forceActiveFocus()
{
    if (const auto rootItem = m_view->rootObject())
        rootItem->forceActiveFocus();
}

void QnWorkbenchWelcomeScreen::connectToSystemInternal(
    const QString& systemId,
    const nx::utils::Url& serverUrl,
    const nx::vms::common::Credentials& credentials,
    bool storePassword,
    bool autoLogin,
    const nx::utils::SharedGuardPtr& completionTracker)
{
    if (!connectingToSystem().isEmpty())
        return; //< Connection process is in progress

    // TODO: #ynikitenkov add look after connection process
    // and don't allow to connect to two or more servers simultaneously
    const auto connectFunction =
        [this, serverUrl, credentials, storePassword, autoLogin, systemId, completionTracker]()
        {
            setConnectingToSystem(systemId);

            auto url = serverUrl;
            if (!credentials.password.isEmpty())
                url.setPassword(credentials.password);
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

void QnWorkbenchWelcomeScreen::connectToCloudSystem(
    const QString& systemId,
    const QString& serverUrl)
{
    if (!isLoggedInToCloud())
        return;

    const bool autoLogin = qnCloudStatusWatcher->stayConnected();
    connectToSystemInternal(systemId, serverUrl,
        qnCloudStatusWatcher->credentials(), false, autoLogin);
}

void QnWorkbenchWelcomeScreen::connectToAnotherSystem()
{
    menu()->trigger(action::OpenLoginDialogAction);
}

void QnWorkbenchWelcomeScreen::setupFactorySystem(const QString& serverUrl)
{
    setVisibleControls(false);
    const auto controlsGuard = nx::utils::makeSharedGuard(
        [this]() { setVisibleControls(true); });

    const auto showDialogHandler = [this, serverUrl, controlsGuard]()
        {
            /* We are receiving string with port but without protocol, so we must parse it. */
            const auto dialog = new QnSetupWizardDialog(mainWindowWidget());
            dialog->setAttribute(Qt::WA_DeleteOnClose);

            const auto url = nx::utils::Url(serverUrl);

            dialog->setUrl(url.toQUrl());
            if (isLoggedInToCloud())
                dialog->setCloudCredentials(qnCloudStatusWatcher->credentials());

            connect(dialog, &QDialog::accepted, this,
                [this, dialog, url, controlsGuard]()
                {
                    static constexpr bool kNoAutoLogin = false;
                    if (dialog->localCredentials().isValid())
                    {
                        connectToSystemInternal(QString(), url, dialog->localCredentials(),
                            dialog->savePassword(), kNoAutoLogin, controlsGuard);
                    }
                    else if (dialog->cloudCredentials().isValid())
                    {
                        const auto cloudCredentials = dialog->cloudCredentials();

                        if (dialog->savePassword())
                        {
                            nx::vms::client::core::helpers::saveCloudCredentials(cloudCredentials);
                        }

                        qnCloudStatusWatcher->setCredentials(cloudCredentials, true);
                        connectToSystemInternal(QString(), url, cloudCredentials,
                            dialog->savePassword(), kNoAutoLogin, controlsGuard);
                    }
                });

            if (!ini().modalServerSetupWizard)
            {
                dialog->loadPage();
                dialog->show();
            }
            else
            {
                dialog->exec();
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

void QnWorkbenchWelcomeScreen::hideSystem(const QString& systemId, const QString& localSystemId)
{
    const auto localSystemUuid = QnUuid::fromStringSafe(localSystemId);
    NX_ASSERT(!localSystemUuid.isNull());
    if (localSystemUuid.isNull())
        return;

    qnSystemWeightsManager->setWeight(localSystemUuid, 0); //< Moves system to the end of tile's list.
    nx::vms::client::core::helpers::removeCredentials(localSystemUuid);
    nx::vms::client::core::helpers::removeConnection(localSystemUuid);

    if (const auto system = qnSystemsFinder->getSystem(systemId))
    {
        auto knownConnections = qnClientCoreSettings->knownServerConnections();
        const auto moduleManager = commonModule()->moduleDiscoveryManager();
        const auto servers = system->servers();
        for (const auto info: servers)
        {
            const auto moduleId = info.id;
            moduleManager->forgetModule(moduleId);

            const auto itEnd = std::remove_if(knownConnections.begin(), knownConnections.end(),
                [moduleId](const QnClientCoreSettings::KnownServerConnection& connection)
                {
                    return moduleId == connection.serverId;
                });
            knownConnections.erase(itEnd, knownConnections.end());
        }
        qnClientCoreSettings->setKnownServerConnections(knownConnections);
    }

    qnClientCoreSettings->save();
}
