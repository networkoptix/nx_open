// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "welcome_screen.h"

#include <QtCore/QMimeData>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>

#include <api/runtime_info_manager.h>
#include <client/client_runtime_settings.h>
#include <client/client_settings.h>
#include <client/forgotten_systems_manager.h>
#include <client/system_weights_manager.h>
#include <client_core/client_core_module.h>
#include <client_core/client_core_settings.h>
#include <common/common_module.h>
#include <core/resource/file_processor.h>
#include <core/resource/resource.h>
#include <finders/systems_finder.h>
#include <helpers/system_helpers.h>
#include <network/system_helpers.h>
#include <nx/branding.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/async_http_client_reply.h>
#include <nx/network/rest/result.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/remote_connection_error.h>
#include <nx/vms/client/core/network/remote_connection_error_strings.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/settings/systems_visibility_manager.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/connection_url_parser.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/style/svg_icon_provider.h>
#include <nx/vms/client/desktop/system_logon/data/logon_data.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx/vms/discovery/manager.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_handler.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/models/systems_controller.h>
#include <ui/statistics/modules/certificate_statistics_module.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>
#include <utils/common/util.h>
#include <watchers/cloud_status_watcher.h>

#include "../data/connect_tiles_proxy_model.h"
#include "../data/systems_visibility_sort_filter_model.h"
#include "login_dialog.h"
#include "setup_wizard_dialog.h"

using CredentialsManager = nx::vms::client::core::CredentialsManager;
using RemoteConnectionErrorCode = nx::vms::client::core::RemoteConnectionErrorCode;

namespace {

const int kDefaultSimpleModeTilesNumber = 6;
const std::chrono::milliseconds kSystemConnectTimeout = std::chrono::seconds(12);

QnResourceList extractResources(const QList<QUrl>& urls, QnResourcePool* resourcePool)
{
    return QnFileProcessor::createResourcesForFiles(
        QnFileProcessor::findAcceptedFiles(urls),
        resourcePool);
}

} // namespace

namespace nx::vms::client::desktop {

WelcomeScreen::WelcomeScreen(QWidget* parent):
    base_type(qnClientCoreModule->mainQmlEngine(), parent),
    QnWorkbenchContextAware(parent)
{
    NX_ASSERT(qnRuntime->isDesktopMode());

    setPaletteColor(this, QPalette::Window, colorTheme()->color("dark3"));

    rootContext()->setContextProperty("workbench", workbench());
    rootContext()->setContextProperty(lit("context"), this);
    setSource(lit("Nx/WelcomeScreen/WelcomeScreen.qml"));
    setResizeMode(QQuickWidget::SizeRootObjectToView);

    if (status() == QQuickWidget::Error)
    {
        for (const auto& error: errors())
            NX_ERROR(this, error.toString());
        NX_CRITICAL(false, "Welcome screen loading failed.");
    }

    NX_CRITICAL(qnCloudStatusWatcher, "Cloud watcher does not exist");
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::cloudLoginChanged,
        this, &WelcomeScreen::cloudUserNameChanged);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged, this,
        [this]()
        {
            if (m_systemModel)
            {
                m_systemModel->setLoggedToCloud(
                    qnCloudStatusWatcher->status() != QnCloudStatusWatcher::LoggedOut);
            }
            emit hasCloudConnectionIssueChanged();
        });
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::isCloudEnabledChanged,
        this, &WelcomeScreen::isCloudEnabledChanged);

    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::is2FaEnabledForUserChanged,
        this, &WelcomeScreen::is2FaEnabledForUserChanged);

    setHelpTopic(this, Qn::Login_Help);

    connect(qnSettings, &QnClientSettings::valueChanged, this,
        [this](int valueId)
        {
            if (valueId == QnClientSettings::AUTO_LOGIN)
                emit resetAutoLogin();
        });

    createSystemModel();
}

void WelcomeScreen::resizeEvent(QResizeEvent* event)
{
    base_type::resizeEvent(event);

    // Resize offscreen QQuickWindow content item here because
    // it does not get resized when offscreen QQuickWindow size is changed.
    // This content item is a parent for popup mouse grab overlay.
    // The overlay should be the same size as the window in order to
    // correctly hide popups when mouse button is pressed.
    if (auto contentItem = quickWindow()->contentItem())
        contentItem->setSize(quickWindow()->size());
}

WelcomeScreen::~WelcomeScreen()
{
    for (auto request: m_runningRequests)
        request->pleaseStopSync();
    m_runningRequests.clear();
}

void WelcomeScreen::connectionToSystemEstablished(const QnUuid& systemId)
{
    emit closeTile(systemId.toString());
}

QObject* WelcomeScreen::systemModel()
{
    return m_systemModel;
}

void WelcomeScreen::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);

    action(ui::action::EscapeHotkeyAction)->setEnabled(false);
    emit gridEnabledChanged();
}

void WelcomeScreen::hideEvent(QHideEvent* event)
{
    base_type::hideEvent(event);

    setGlobalPreloaderVisible(false); //< Auto toggle off preloader

    action(ui::action::EscapeHotkeyAction)->setEnabled(true);
    emit gridEnabledChanged();
}

void WelcomeScreen::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange)
        quickWindow()->setColor(palette().color(QPalette::Window));

    base_type::changeEvent(event);
}

bool WelcomeScreen::hasCloudConnectionIssue() const
{
    return qnCloudStatusWatcher->status() == QnCloudStatusWatcher::Status::Offline;
}

QString WelcomeScreen::cloudUserName() const
{
    return qnCloudStatusWatcher->cloudLogin();
}

bool WelcomeScreen::isCloudEnabled() const
{
    return qnCloudStatusWatcher->isCloudEnabled();
}

bool WelcomeScreen::visibleControls() const
{
    return m_visibleControls;
}

void WelcomeScreen::setVisibleControls(bool visible)
{
    if (m_visibleControls == visible)
        return;

    m_visibleControls = visible;
    emit visibleControlsChanged();
}

bool WelcomeScreen::gridEnabled() const
{
    return !isHidden();
}

void WelcomeScreen::openConnectingTile(std::optional<RemoteConnectionErrorCode> errorCode)
{
    const auto systemId = m_connectingSystemId;

    QString errorMessage;

    // If this parameter is true, login field will be highlighted with a red frame, otherwise it
    // will be the host field.
    const bool isLoginError = !systemId.isEmpty()
        && errorCode
        && (
            *errorCode == RemoteConnectionErrorCode::unauthorized
            || *errorCode == RemoteConnectionErrorCode::loginAsCloudUserForbidden
            || *errorCode == RemoteConnectionErrorCode::sessionExpired
            || *errorCode == RemoteConnectionErrorCode::cloudSessionExpired
            || *errorCode == RemoteConnectionErrorCode::userIsLockedOut
            );

    m_connectingSystemId.clear(); //< Connection process is finished.

    if (!systemId.isEmpty() && errorCode)
        errorMessage = core::shortErrorDescription(*errorCode);

    executeLater(
        [this, systemId, errorMessage, isLoginError]()
        {
            emit openTile(systemId, errorMessage, isLoginError);
        },
        this);
}

bool WelcomeScreen::connectingTileExists() const
{
    return !m_connectingSystemId.isEmpty();
}

void WelcomeScreen::setConnectingToSystem(const QString& value)
{
    m_connectingSystemId = value;
}

bool WelcomeScreen::globalPreloaderVisible() const
{
    return m_globalPreloaderVisible;
}

void WelcomeScreen::setGlobalPreloaderVisible(bool value)
{
    if (value == m_globalPreloaderVisible)
        return;

    m_globalPreloaderVisible = value;
    emit globalPreloaderVisibleChanged();
}

bool WelcomeScreen::globalPreloaderEnabled() const
{
    return m_globalPreloaderEnabled;
}

void WelcomeScreen::setGlobalPreloaderEnabled(bool value)
{
    if (value == m_globalPreloaderEnabled)
        return;

    m_globalPreloaderEnabled = value;
    emit globalPreloaderEnabledChanged();
}

void WelcomeScreen::setMessage(const QString& message)
{
    if (m_message == message)
        return;

    m_message = message;

    emit messageChanged();

    // Repainting the widget to guarantee that started screen recording won't contain the visible
    // message.
    // TODO: Find a better way to achieve the same effect.
    update();
    repaint();

    qApp->flush();
    qApp->sendPostedEvents();
}

QString WelcomeScreen::message() const
{
    return m_message;
}

int WelcomeScreen::simpleModeTilesNumber() const
{
    return ini().simpleModeTilesNumber == -1
        ? kDefaultSimpleModeTilesNumber
        : ini().simpleModeTilesNumber;
}

bool WelcomeScreen::richTextEnabled() const
{
    return ini().developerMode;
}

bool WelcomeScreen::confirmCloudTileHiding() const
{
    auto dialog = new QnMessageBox(
        QnMessageBox::Icon::Question,
        tr("Are you sure you want to hide this tile?"),
        tr("You will be able to access the %1 login menu at any time by clicking "
           "the cloud icon on the navigation panel.").arg(nx::branding::cloudName()),
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        parentWidget());
    dialog->addButton(tr("Hide"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);

    return dialog->exec() != QDialogButtonBox::Cancel;
}

void WelcomeScreen::openHelp() const
{
    QnHelpHandler::openHelpTopic(Qn::MainWindow_TitleBar_MainMenu_Help);
}

QString WelcomeScreen::minSupportedVersion() const
{
    return nx::vms::common::ServerCompatibilityValidator::minimalVersion().toString();
}

void WelcomeScreen::showSystemWentToOfflineNotification() const
{
    // Synchronous call leads to potential QML binding loops.
    executeDelayed(
        [this]
        {
            QnMessageBox::critical(parentWidget(),
                tr("Unable to connect to the system because it has become offline"));
        });
}

bool WelcomeScreen::checkUrlIsValid(const QString& urlText) const
{
    QUrl url(urlText);
    return url.isValid() && !url.scheme().isEmpty();
}

void WelcomeScreen::abortConnectionProcess() const
{
    menu()->trigger(ui::action::DisconnectAction, {Qn::ForceRole, true});
}

bool WelcomeScreen::isAcceptableDrag(const QList<QUrl>& urls)
{
    return !extractResources(urls, resourcePool()).isEmpty();
}

void WelcomeScreen::makeDrop(const QList<QUrl>& urls)
{
    const auto resources = extractResources(urls, resourcePool());
    if (resources.isEmpty())
        return;

    if (menu()->triggerIfPossible(ui::action::DropResourcesAction, resources))
        action(ui::action::ResourcesModeAction)->setChecked(true);
}

void WelcomeScreen::connectToLocalSystem(
    const QString& systemId,
    const QString& serverUrl,
    const QString& serverId,
    const QString& userName,
    const QString& password,
    bool storePassword)
{
    nx::utils::Url url = parseConnectionUrlFromUserInput(serverUrl);

    connectToSystemInternal(
        systemId,
        serverId.isEmpty() ? std::nullopt : std::optional{QnUuid::fromStringSafe(serverId)},
        nx::network::SocketAddress(url.host(), url.port()),
        nx::network::http::PasswordCredentials{userName.toStdString(), password.toStdString()},
        storePassword);
}

void WelcomeScreen::connectToLocalSystemUsingSavedPassword(
    const QString& systemId,
    const QString& serverUrl,
    const QString& serverId,
    const QString& userName,
    bool storePassword)
{
    const auto system = qnSystemsFinder->getSystem(systemId);
    NX_ASSERT(system, "System is empty");
    if (!system)
        return;

    const QnUuid localSystemId = system->localId();

    const auto credentials = CredentialsManager::credentials(
        localSystemId,
        userName.toStdString());

    if (!NX_ASSERT(credentials))
        return;

    nx::utils::Url url = parseConnectionUrlFromUserInput(serverUrl);
    connectToSystemInternal(
        systemId,
        serverId.isEmpty() ? std::nullopt : std::optional{QnUuid::fromStringSafe(serverId)},
        nx::network::SocketAddress::fromUrl(url),
        *credentials,
        storePassword);
}

void WelcomeScreen::forgetUserPassword(
    const QString& localSystemId,
    const QString& user)
{
    const auto localId = QnUuid::fromStringSafe(localSystemId);
    if (!localId.isNull() && !user.isEmpty())
        CredentialsManager::forgetStoredPassword(localId, user.toStdString());
}

void WelcomeScreen::forgetAllCredentials(const QString& localSystemId)
{
    const auto localId = QnUuid::fromStringSafe(localSystemId);
    if (!localId.isNull())
        CredentialsManager::removeCredentials(localId);
}

void WelcomeScreen::forceActiveFocus()
{
    if (const auto rootItem = rootObject())
        rootItem->forceActiveFocus();
}

bool WelcomeScreen::isLoggedInToCloud() const
{
    return (qnCloudStatusWatcher->status() != QnCloudStatusWatcher::LoggedOut);
}

static SystemsVisibilitySortFilterModel* createVisibilityModel(QObject* parent)
{
    using TileScopeFilter = nx::vms::client::core::welcome_screen::TileScopeFilter;

    auto visibilityModel = new SystemsVisibilitySortFilterModel(parent);

    NX_CRITICAL(qnForgottenSystemsManager,
        "Client forgotten systems manager is not initialized yet");
    NX_CRITICAL(qnClientCoreSettings,
        "Client core settings is not initialized yet");

    visibilityModel->setVisibilityScopeFilterCallbacks(
        []()
        {
            return static_cast<TileScopeFilter>(
                qnClientCoreSettings->tileVisibilityScopeFilter());
        },
        [](TileScopeFilter filter)
        {
            qnClientCoreSettings->setTileVisibilityScopeFilter(filter);
            qnClientCoreSettings->save();
        }
    );

    visibilityModel->setForgottenCheckCallback(
        [](const QString& systemId)
        {
            return qnForgottenSystemsManager->isForgotten(systemId);
        }
    );

    QObject::connect(qnForgottenSystemsManager, &QnForgottenSystemsManager::forgottenSystemAdded,
        visibilityModel, &SystemsVisibilitySortFilterModel::forgottenSystemAdded);

    QObject::connect(qnForgottenSystemsManager, &QnForgottenSystemsManager::forgottenSystemRemoved,
        visibilityModel, &SystemsVisibilitySortFilterModel::forgottenSystemRemoved);

    visibilityModel->setSourceModel(
        new QnSystemsModel(new SystemsController(visibilityModel), visibilityModel));

    return visibilityModel;
}

void WelcomeScreen::createSystemModel()
{
    using TileVisibilityScope = nx::vms::client::core::welcome_screen::TileVisibilityScope;

    m_systemModel = new ConnectTilesProxyModel(this);

    m_systemModel->setCloudVisibilityScopeCallbacks(
        []()
        {
            return qnSystemsVisibilityManager->cloudTileScope();
        },
        [](TileVisibilityScope value)
        {
            qnSystemsVisibilityManager->setCloudTileScope(value);
        }
    );

    m_systemModel->setSourceModel(createVisibilityModel(m_systemModel));

    m_systemModel->setLoggedToCloud(
        qnCloudStatusWatcher->status() != QnCloudStatusWatcher::LoggedOut);

    emit systemModelChanged();
}

void WelcomeScreen::connectToSystemInternal(
    const QString& systemId, //< Actually that's a tile id.
    const std::optional<QnUuid>& serverId,
    const nx::network::SocketAddress& address,
    const nx::network::http::Credentials& credentials,
    bool storePassword,
    const nx::utils::SharedGuardPtr& completionTracker)
{
    if (!m_connectingSystemId.isEmpty())
        return; //< Connection process is in progress

    NX_DEBUG(this, "Delayed connect to the system %1 after click on tile", address);
    if (context()->user())
        appContext()->clientStateHandler()->saveWindowsConfiguration();

    LogonData logonData(core::LogonData{
        .address = address,
        .credentials = credentials,
        .expectedServerId = serverId});
    logonData.storePassword = storePassword;
    logonData.connectScenario = ConnectScenario::connectFromTile;

    // TODO: #ynikitenkov add look after connection process
    // and don't allow to connect to two or more servers simultaneously
    const auto connectFunction =
        [this, systemId, logonData, completionTracker]()
        {
            setConnectingToSystem(systemId);
            NX_DEBUG(this, "Connecting to the system %1 after click on tile",
                logonData.address);
            menu()->trigger(ui::action::ConnectAction,
                ui::action::Parameters().withArgument(Qn::LogonDataRole, logonData));
        };

    // We have to use delayed execution to prevent client crash when stopping server that we are
    // connecting to.
    executeLater(connectFunction, this);
}

void WelcomeScreen::connectToCloudSystem(const QString& systemId)
{
    if (!isLoggedInToCloud())
        return;

    setConnectingToSystem(systemId);
    CloudSystemConnectData connectData{systemId, ConnectScenario::connectFromTile};
    menu()->trigger(ui::action::ConnectToCloudSystemAction,
        ui::action::Parameters().withArgument(Qn::CloudSystemConnectDataRole, connectData));
}

void WelcomeScreen::connectToAnotherSystem()
{
    menu()->trigger(ui::action::OpenLoginDialogAction);
}

void WelcomeScreen::testSystemOnline(const QString& serverUrl)
{
    if (const auto manager = appContext()->moduleDiscoveryManager())
    {
        const auto url = parseConnectionUrlFromUserInput(serverUrl);
        if (url.isValid())
            manager->checkEndpoint(url);
    }
}

void WelcomeScreen::storeCredentialsWithoutConnection(
    const nx::network::SocketAddress& address,
    const nx::network::http::Credentials& credentials)
{
    auto client =
        nx::network::http::AsyncHttpClient::create(nx::network::ssl::kAcceptAnyCertificate);
    // TODO: #sivanov Do we still have to force authentication type here? Request is unathorized.
    client->setAuthType(nx::network::http::AuthType::authBasicAndDigest);

    // First connection to a system may take a long time because it may require hole punching.
    client->setSendTimeoutMs(kSystemConnectTimeout.count());
    client->setResponseReadTimeoutMs(kSystemConnectTimeout.count());
    m_runningRequests.push_back(client);

    QPointer<QObject> guard(this);

    const auto handleReply =
        [this, guard, address, credentials](nx::network::http::AsyncHttpClientPtr reply)
        {
             executeInThread(guard->thread(),
                [this, guard, address, credentials, reply]
                {
                    if (!guard)
                        return;

                    if (reply->failed())
                    {
                        NX_DEBUG(this, nx::format("Factory system <%1>: failed reply"), address);
                    }
                    else
                    {
                        const auto data = reply->fetchMessageBodyBuffer();

                        nx::network::rest::JsonResult jsonReply;
                        if (!QJson::deserialize(data, &jsonReply))
                        {
                            NX_DEBUG(this,
                                "Factory system <%1>: failed to deserialize json reply:\n%2",
                                address, data);
                            return;
                        }

                        nx::vms::api::ModuleInformation moduleInformation;
                        if (!QJson::deserialize(jsonReply.reply, &moduleInformation))
                        {
                            NX_DEBUG(this,
                                "Factory system <%1>: failed to deserialize module information:\n%2",
                                address, data);
                            return;
                        }

                        const auto localSystemId = helpers::getLocalSystemId(moduleInformation);
                        CredentialsManager::storeCredentials(localSystemId, credentials);
                    }

                    m_runningRequests.removeOne(reply);
                }); //< executeInThread

            reply->pleaseStopSync();
        }; //< handleReply

    const auto url = nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setEndpoint(address)
        .setPath("/api/moduleInformation")
        .toUrl();
    client->doGet(url, handleReply);
}

void WelcomeScreen::setupFactorySystem(
    const QString& systemId,
    const QString& serverUrl,
    const QString& serverId)
{
    connectToLocalSystem(
        systemId,
        serverUrl,
        serverId,
        helpers::kFactorySystemUser,
        helpers::kFactorySystemPassword,
        /*storePassword*/ false);
}

void WelcomeScreen::setupFactorySystem(
    const nx::network::SocketAddress& address,
    const QnUuid& serverId)
{
    setVisibleControls(false);
    const auto controlsGuard = nx::utils::makeSharedGuard(
        [this]() { setVisibleControls(true); });

    const auto showDialogHandler = [this, address, serverId, controlsGuard]()
        {
            /* We are receiving string with port but without protocol, so we must parse it. */
            const auto dialog = new SetupWizardDialog(address, serverId, mainWindowWidget());
            dialog->setAttribute(Qt::WA_DeleteOnClose);

            const auto saveCredentials =
                [this, dialog, address, controlsGuard]()
                {
                    if (!dialog->savePassword())
                        return;

                    const auto credentials = nx::network::http::PasswordCredentials(
                        helpers::kFactorySystemUser.toStdString(),
                        dialog->password().toStdString());
                    storeCredentialsWithoutConnection(address, credentials);
                };

            const auto connectToSystem =
                [this, dialog, address, serverId, controlsGuard]()
                {
                    const auto credentials = nx::network::http::PasswordCredentials(
                        helpers::kFactorySystemUser.toStdString(),
                        dialog->password().toStdString());
                    connectToSystemInternal(
                        /*systemId*/ QString(),
                        serverId,
                        address,
                        credentials,
                        dialog->savePassword(),
                        controlsGuard);
                };

            connect(dialog, &QDialog::rejected, this, saveCredentials);
            connect(dialog, &QDialog::accepted, this, connectToSystem);

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

void WelcomeScreen::logoutFromCloud()
{
    menu()->trigger(ui::action::LogoutFromCloud);
}

void WelcomeScreen::manageCloudAccount()
{
    menu()->trigger(ui::action::OpenCloudManagementUrl);
}

void WelcomeScreen::loginToCloud()
{
    menu()->trigger(ui::action::LoginToCloud);
}

void WelcomeScreen::createAccount()
{
    menu()->trigger(ui::action::OpenCloudRegisterUrl);
}

void WelcomeScreen::deleteSystem(const QString& systemId, const QString& localSystemId)
{
    const auto localSystemUuid = QnUuid::fromStringSafe(localSystemId);
    NX_ASSERT(!localSystemUuid.isNull());
    if (localSystemUuid.isNull())
        return;

    qnForgottenSystemsManager->forgetSystem(systemId);
    qnForgottenSystemsManager->forgetSystem(localSystemId);

    CredentialsManager::removeCredentials(localSystemUuid);
    nx::vms::client::core::helpers::removeConnection(localSystemUuid);

    qnSystemsVisibilityManager->removeSystemData(localSystemUuid);

    if (const auto system = qnSystemsFinder->getSystem(systemId))
    {
        auto knownConnections = qnClientCoreSettings->knownServerConnections();
        const auto moduleManager = appContext()->moduleDiscoveryManager();
        const auto servers = system->servers();
        for (const auto& info: servers)
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

bool WelcomeScreen::saveCredentialsAllowed() const
{
    return qnSettings->saveCredentialsAllowed();
}

bool WelcomeScreen::is2FaEnabledForUser() const
{
    return qnCloudStatusWatcher->is2FaEnabledForUser();
}

} // namespace nx::vms::client::desktop
