#include "context.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QClipboard>

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
#include <watchers/available_cameras_watcher.h>
#include <watchers/cloud_status_watcher.h>
#include <watchers/user_watcher.h>
#include <helpers/cloud_url_helper.h>
#include <helpers/nx_globals_object.h>
#include <settings/last_connection.h>
#include <nx/utils/url_builder.h>

using namespace nx::vms::utils;

namespace {

const auto kUserRightsRefactoredVersion = QnSoftwareVersion(3, 0);

} // anonymous namespace

QnContext::QnContext(QObject* parent) :
    base_type(parent),
    m_nxGlobals(new NxGlobalsObject(this)),
    m_connectionManager(new QnConnectionManager(this)),
    m_appInfo(new QnMobileAppInfo(this)),
    m_uiController(new QnMobileClientUiController(this)),
    m_cloudUrlHelper(new QnCloudUrlHelper(
        SystemUri::ReferralSource::MobileClient,
        SystemUri::ReferralContext::WelcomePage,
        this)),
    m_localPrefix(lit("qrc:///"))
{
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
        const bool useLayouts = m_connectionManager->connectionVersion() < kUserRightsRefactoredVersion;
        auto camerasWatcher = qnCommon->instance<QnAvailableCamerasWatcher>();
        camerasWatcher->setUseLayouts(useLayouts);
    });
}

QnContext::~QnContext() {}

QnCloudStatusWatcher* QnContext::cloudStatusWatcher() const
{
    return qnCommon->instance<QnCloudStatusWatcher>();
}

QnUserWatcher* QnContext::userWatcher() const
{
    return qnCommon->instance<QnUserWatcher>();
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

int QnContext::getStatusBarHeight() const {
    return statusBarHeight();
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
    return QnTextureSizeHelper::instance()->maxTextureSize();
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
    emit autoLoginEnabledChanged();
}

bool QnContext::testMode() const
{
    return qnSettings->testMode();
}

QString QnContext::initialTest() const
{
    return qnSettings->initialTest();
}

void QnContext::removeSavedConnection(const QString& localSystemId)
{
    const auto localId = QnUuid::fromStringSafe(localSystemId);

    NX_ASSERT(!localId.isNull());
    if (localId.isNull())
        return;

    auto recentConnections = qnClientCoreSettings->recentLocalConnections();
    recentConnections.remove(localId);
    qnClientCoreSettings->setRecentLocalConnections(recentConnections);

    auto authenticationData = qnClientCoreSettings->systemAuthenticationData();
    authenticationData.remove(localId);
    qnClientCoreSettings->setSystemAuthenticationData(authenticationData);

    qnClientCoreSettings->setRecentLocalConnections(recentConnections);
    qnClientCoreSettings->save();
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

QUrl QnContext::getInitialUrl() const
{
    return qnSettings->startupParameters().url;
}

QUrl QnContext::getWebSocketUrl() const
{
    const auto port = qnSettings->webSocketPort();
    if (port == 0)
        return QUrl();

    return nx::utils::UrlBuilder()
        .setScheme(lit("ws"))
        .setHost(lit("localhost"))
        .setPort(port);
}

void QnContext::setCloudCredentials(const QString& login, const QString& password)
{
    //TODO: #GDM do we need store temporary credentials here?
    qnClientCoreSettings->setCloudLogin(login);
    qnClientCoreSettings->setCloudPassword(password);
    cloudStatusWatcher()->setCredentials(QnCredentials(login, password));
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
