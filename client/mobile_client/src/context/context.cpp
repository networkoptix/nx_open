#include "context.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QClipboard>

#include <camera/camera_thumbnail_cache.h>
#include <utils/mobile_app_info.h>
#include <common/common_module.h>
#include <context/connection_manager.h>
#include <context/context_settings.h>
#include <ui/window_utils.h>
#include <ui/texture_size_helper.h>
#include <ui/models/recent_local_connections_model.h>
#include <client_core/client_core_settings.h>
#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_app_info.h>
#include <mobile_client/mobile_client_ui_controller.h>
#include <watchers/available_cameras_watcher.h>
#include <watchers/cloud_status_watcher.h>
#include <watchers/user_watcher.h>
#include <helpers/cloud_url_helper.h>

using namespace nx::vms::utils;

namespace {

const auto kUserRightsRefactoredVersion = QnSoftwareVersion(3, 0);

} // anonymous namespace

QnContext::QnContext(QObject* parent) :
    base_type(parent),
    m_connectionManager(new QnConnectionManager(this)),
    m_appInfo(new QnMobileAppInfo(this)),
    m_settings(new QnContextSettings(this)),
    m_uiController(new QnMobileClientUiController(this)),
    m_cloudUrlHelper(new QnCloudUrlHelper(
        SystemUri::ReferralSource::MobileClient,
        SystemUri::ReferralContext::WelcomePage,
        this))
{
    connect(m_connectionManager, &QnConnectionManager::connectionStateChanged,
            this, [this]()
    {
        auto thumbnailCache = QnCameraThumbnailCache::instance();
        if (!thumbnailCache)
            return;

        if (m_connectionManager->connectionState() == QnConnectionManager::Connected)
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

bool QnContext::testMode() const
{
    return qnSettings->testMode();
}

QString QnContext::initialTest() const
{
    return qnSettings->initialTest();
}

void QnContext::removeSavedConnection(const QString& systemName)
{
    auto lastConnections = qnClientCoreSettings->recentLocalConnections();

    auto connectionEqual = [systemName](const QnLocalConnectionData& connection)
    {
        return connection.systemName == systemName;
    };
    lastConnections.erase(std::remove_if(lastConnections.begin(), lastConnections.end(), connectionEqual),
                          lastConnections.end());

    qnClientCoreSettings->setRecentLocalConnections(lastConnections);
    qnClientCoreSettings->save();
}

void QnContext::setLastUsedConnection(const QString& systemId, const QUrl& url)
{
    qnSettings->setLastUsedSystemId(systemId);
    QUrl clearedUrl = url;
    clearedUrl.setPassword(QString());
    qnSettings->setLastUsedUrl(clearedUrl);
}

void QnContext::clearLastUsedConnection()
{
    qnSettings->setLastUsedSystemId(QString());
    qnSettings->setLastUsedUrl(QUrl());
}

QString QnContext::getLastUsedSystemId() const
{
    return qnSettings->lastUsedSystemId();
}

QString QnContext::getLastUsedUrl() const
{
    QUrl url = qnSettings->lastUsedUrl();

    if (!url.isValid() || url.userName().isEmpty())
        return QString();

    if (url.password().isEmpty())
    {
        QnRecentLocalConnectionsModel connectionsModel;
        connectionsModel.setSystemName(getLastUsedSystemId());
        if (!connectionsModel.hasConnections())
            return QString();

        const auto firstIndex = connectionsModel.index(0);
        const auto password = connectionsModel.data(
            firstIndex, QnRecentLocalConnectionsModel::PasswordRole).toString();
        if (password.isEmpty())
            return QString();

        url.setPassword(password);
    }

    return url.toString();
}

void QnContext::setCloudCredentials(const QString& login, const QString& password)
{
    //TODO: #GDM do we need store temporary credentials here?
    qnClientCoreSettings->setCloudLogin(login);
    qnClientCoreSettings->setCloudPassword(password);
    cloudStatusWatcher()->setCloudCredentials(QnCredentials(login, password));
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
