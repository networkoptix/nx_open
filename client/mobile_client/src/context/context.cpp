#include "context.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QClipboard>
#include <QtGui/QScreen>

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
#include <watchers/user_watcher.h>
#include <helpers/cloud_url_helper.h>
#include <helpers/nx_globals_object.h>
#include <helpers/system_helpers.h>
#include <settings/last_connection.h>
#include <settings/qml_settings_adaptor.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/address_resolver.h>

using namespace nx::vms::utils;

namespace {

const auto kUserRightsRefactoredVersion = QnSoftwareVersion(3, 0);

} // anonymous namespace

QnContext::QnContext(QObject* parent) :
    base_type(parent),
    m_nxGlobals(new NxGlobalsObject(this)),
    m_connectionManager(new QnConnectionManager(this)),
    m_settings(new QmlSettingsAdaptor(this)),
    m_appInfo(new QnMobileAppInfo(this)),
    m_uiController(new QnMobileClientUiController(this)),
    m_cloudUrlHelper(new QnCloudUrlHelper(
        SystemUri::ReferralSource::MobileClient,
        SystemUri::ReferralContext::WelcomePage,
        this)),
    m_localPrefix(lit("qrc:///"))
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
}

QnContext::~QnContext() {}

QnCloudStatusWatcher* QnContext::cloudStatusWatcher() const
{
    return qnMobileClientModule->cloudStatusWatcher();
}

QnUserWatcher* QnContext::userWatcher() const
{
    return commonModule()->instance<QnUserWatcher>();
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

void QnContext::removeSavedConnection(const QString& localSystemId, const QString& userName)
{
    using namespace nx::client::core::helpers;

    const auto localId = QnUuid::fromStringSafe(localSystemId);

    NX_ASSERT(!localId.isNull());
    if (localId.isNull())
        return;

    removeCredentials(localId, userName);

    if (userName.isEmpty() || !hasCredentials(localId))
        removeConnection(localId);

    qnClientCoreSettings->save();
}

void QnContext::clearSavedPasswords()
{
    nx::client::core::helpers::clearSavedPasswords();
}

void QnContext::clearLastUsedConnection()
{
    qnSettings->setLastUsedConnection(LastConnectionData());
}

QString QnContext::getLastUsedSystemName() const
{
    return qnSettings->lastUsedConnection().systemName;
}

QUrl QnContext::getLastUsedUrl() const
{
    return qnSettings->lastUsedConnection().urlWithCredentials();
}

bool QnContext::isCloudConnectionUrl(const QUrl& url)
{
    return url.scheme() == QnConnectionManager::kCloudConnectionScheme;
}

QUrl QnContext::getInitialUrl() const
{
    return qnSettings->startupParameters().url;
}

QUrl QnContext::getWebSocketUrl() const
{
    const auto port = qnSettings->webSocketPort();
    if (port == 0)
        return QUrl();

    return nx::network::url::Builder()
        .setScheme(lit("ws"))
        .setHost(lit("localhost"))
        .setPort(port);
}

void QnContext::setCloudCredentials(const QString& login, const QString& password)
{
    // TODO: #GDM do we need store temporary credentials here?
    qnClientCoreSettings->setCloudLogin(login);
    qnClientCoreSettings->setCloudPassword(password);
    cloudStatusWatcher()->setCredentials(QnEncodedCredentials(login, password));
    qnClientCoreSettings->save();
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
