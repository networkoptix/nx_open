#include "context.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QClipboard>
#include <QtGui/QScreen>

#include <nx/vms/client/core/settings/client_core_settings.h>

#include <camera/camera_thumbnail_cache.h>
#include <utils/mobile_app_info.h>
#include <common/common_module.h>
#include <context/connection_manager.h>
#include <ui/window_utils.h>
#include <ui/texture_size_helper.h>
#include <client_core/client_core_settings.h>
#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_app_info.h>
#include <mobile_client/mobile_client_ui_controller.h>
#include <mobile_client/mobile_client_module.h>
#include <watchers/available_cameras_watcher.h>
#include <watchers/cloud_status_watcher.h>
#include <helpers/cloud_url_helper.h>
#include <helpers/system_helpers.h>
#include <settings/last_connection.h>
#include <settings/qml_settings_adaptor.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/socket_global.h>
#include <nx/network/address_resolver.h>
#include <nx/client/core/two_way_audio/two_way_audio_mode_controller.h>
#include <nx/client/core/watchers/user_watcher.h>
#include <nx/client/core/utils/operation_manager.h>
#include <nx/vms/discovery/manager.h>
#include <finders/systems_finder.h>
#include <utils/common/delayed.h>
#include <nx/utils/guarded_callback.h>

using namespace nx::vms::utils;

namespace {

static const nx::utils::SoftwareVersion kUserRightsRefactoredVersion(3, 0);

} // namespace

QnContext::QnContext(QObject* parent) :
    base_type(parent),
    m_connectionManager(new QnConnectionManager(this)),
    m_settings(new QmlSettingsAdaptor(this)),
    m_appInfo(new QnMobileAppInfo(this)),
    m_uiController(new QnMobileClientUiController(this)),
    m_cloudUrlHelper(new QnCloudUrlHelper(
        SystemUri::ReferralSource::MobileClient,
        SystemUri::ReferralContext::WelcomePage,
        this)),
    m_localPrefix(lit("qrc:///")),
    m_customMargins()
{
    const auto screen = qApp->primaryScreen();
    screen->setOrientationUpdateMask(Qt::PortraitOrientation | Qt::InvertedPortraitOrientation
        | Qt::LandscapeOrientation | Qt::InvertedLandscapeOrientation);
    connect(screen, &QScreen::orientationChanged, this, &QnContext::deviceStatusBarHeightChanged);

    connect(m_connectionManager, &QnConnectionManager::connectionStateChanged, this,
        [this]()
        {
            auto thumbnailCache = QnCameraThumbnailCache::instance();
            if (!thumbnailCache)
                return;

            if (m_connectionManager->isOnline())
                thumbnailCache->start();
            else
                thumbnailCache->stop();
        });

    connect(m_connectionManager, &QnConnectionManager::connectionVersionChanged,
            this, [this]()
    {
        const bool compatibilityMode =
            m_connectionManager->connectionVersion() < kUserRightsRefactoredVersion;
        const auto camerasWatcher = commonModule()->instance<QnAvailableCamerasWatcher>();
        camerasWatcher->setCompatiblityMode(compatibilityMode);
    });

    connect(qnSettings, &QnMobileClientSettings::valueChanged, this,
        [this](int id)
        {
            switch (id)
            {
                case QnMobileClientSettings::AutoLogin:
                    emit autoLoginEnabledChanged();
                    break;

                case QnMobileClientSettings::ShowCameraInfo:
                    emit showCameraInfoChanged();
                    break;

                default:
                    break;
            }
        });

    auto updateMarginsCallback =
        [this]()
        {
            // We have to update margins after all screen-change animations are finished.
            static constexpr int kUpdateDelayMs = 300;
            const auto callback = [this]() { updateCustomMargins(); };
            executeDelayedParented(nx::utils::guarded(this, callback), kUpdateDelayMs, this);
        };

    connect(qApp->screens().first(), &QScreen::geometryChanged, this, updateMarginsCallback);
}

QnContext::~QnContext() {}

QnCloudStatusWatcher* QnContext::cloudStatusWatcher() const
{
    return qnMobileClientModule->cloudStatusWatcher();
}

QnConnectionManager* QnContext::connectionManager() const
{
    return m_connectionManager;
}

nx::vms::client::core::TwoWayAudioController* QnContext::twoWayAudioController() const
{
    return commonModule()->instance<nx::vms::client::core::TwoWayAudioController>();
}

nx::vms::client::core::OperationManager* QnContext::operationManager() const
{
    return commonModule()->instance<nx::vms::client::core::OperationManager>();
}

nx::vms::client::core::UserWatcher* QnContext::userWatcher() const
{
    return commonModule()->instance<nx::vms::client::core::UserWatcher>();
}

QmlSettingsAdaptor* QnContext::settings() const
{
    return m_settings;
}

void QnContext::quitApplication()
{
    qApp->quit();
}

void QnContext::exitFullscreen() {
    showSystemUi();
}

void QnContext::copyToClipboard(const QString &text)
{
    qApp->clipboard()->setText(text);
}

void QnContext::enterFullscreen() {
    hideSystemUi();
}

int QnContext::deviceStatusBarHeight() const
{
    return ::statusBarHeight();
}

bool QnContext::getNavigationBarIsLeftSide() const
{
    return ::isLeftSideNavigationBar();
}

int QnContext::getNavigationBarHeight() const {
    return navigationBarHeight();
}

bool QnContext::getDeviceIsPhone() const {
    return isPhone();
}

void QnContext::setKeepScreenOn(bool keepScreenOn) {
    ::setKeepScreenOn(keepScreenOn);
}

void QnContext::setScreenOrientation(Qt::ScreenOrientation orientation)
{
    ::setScreenOrientation(orientation);
}

int QnContext::getMaxTextureSize() const
{
    if (const auto textureSizeHelper = QnTextureSizeHelper::instance())
        return textureSizeHelper->maxTextureSize();

    return -1;
}

bool QnContext::liteMode() const
{
    return qnSettings->isLiteClientModeEnabled();
}

bool QnContext::autoLoginEnabled() const
{
    return qnSettings->isAutoLoginEnabled();
}

void QnContext::setAutoLoginEnabled(bool enabled)
{
    auto mode = AutoLoginMode::Auto;
    if (liteMode())
        mode = enabled ? AutoLoginMode::Enabled : AutoLoginMode::Disabled;

    const auto intMode = (int) mode;
    if (intMode == qnSettings->autoLoginMode())
        return;

    qnSettings->setAutoLoginMode(intMode);
}

bool QnContext::showCameraInfo() const
{
    return qnSettings->showCameraInfo();
}

void QnContext::setShowCameraInfo(bool showCameraInfo)
{
    if (showCameraInfo == qnSettings->showCameraInfo())
        return;

    qnSettings->setShowCameraInfo(showCameraInfo);
}

bool QnContext::testMode() const
{
    return qnSettings->testMode();
}

QString QnContext::initialTest() const
{
    return qnSettings->initialTest();
}

void QnContext::removeSavedConnection(
    const QString& systemId,
    const QString& localSystemId,
    const QString& userName)
{
    using namespace nx::vms::client::core::helpers;

    const auto localId = QnUuid::fromStringSafe(localSystemId);

    NX_ASSERT(!localId.isNull());
    if (localId.isNull())
        return;

    removeCredentials(localId, userName);

    if (userName.isEmpty() || !hasCredentials(localId))
    {
        removeConnection(localId);
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
    }

    qnClientCoreSettings->save();
}

void QnContext::clearSavedPasswords()
{
    nx::vms::client::core::helpers::clearSavedPasswords();
}

void QnContext::clearLastUsedConnection()
{
    qnSettings->setLastUsedConnection(LastConnectionData());
}

QString QnContext::getLastUsedSystemName() const
{
    return qnSettings->lastUsedConnection().systemName;
}

nx::utils::Url QnContext::getLastUsedUrl() const
{
    return qnSettings->lastUsedConnection().urlWithCredentials();
}

bool QnContext::isCloudConnectionUrl(const nx::utils::Url& url)
{
    return url.scheme() == QnConnectionManager::kCloudConnectionScheme;
}

nx::utils::Url QnContext::getInitialUrl() const
{
    return qnSettings->startupParameters().url;
}

nx::utils::Url QnContext::getWebSocketUrl() const
{
    const auto port = qnSettings->webSocketPort();
    if (port == 0)
        return nx::utils::Url();

    return nx::network::url::Builder()
        .setScheme(lit("ws"))
        .setHost(lit("localhost"))
        .setPort(port);
}

bool QnContext::setCloudCredentials(const QString& login, const QString& password)
{
    const nx::vms::common::Credentials credentials{login, password};
    nx::vms::client::core::settings()->cloudCredentials = credentials;
    const bool result = cloudStatusWatcher()->setCredentials(credentials);
    return result;
}

QString QnContext::lp(const QString& path) const
{
    if (path.isEmpty())
        return path;

    return m_localPrefix + path;
}

void QnContext::setLocalPrefix(const QString& prefix)
{
    m_localPrefix = prefix;
    if (!m_localPrefix.endsWith(lit("/")))
        m_localPrefix.append(lit("/"));
}

void QnContext::updateCustomMargins()
{
    const auto newMargins = getCustomMargins();
    if (m_customMargins == newMargins)
        return;

    m_customMargins = newMargins;
    emit customMarginsChanged();
}

void QnContext::makeShortVibration()
{
    ::makeShortVibration();
}

int QnContext::leftCustomMargin() const
{
    return m_customMargins.left();
}

int QnContext::rightCustomMargin() const
{
    return m_customMargins.right();
}

int QnContext::topCustomMargin() const
{
    return m_customMargins.top();
}

int QnContext::bottomCustomMargin() const
{
    return m_customMargins.bottom();
}
