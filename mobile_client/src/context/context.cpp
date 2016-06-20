#include "context.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QClipboard>

#include <camera/camera_thumbnail_cache.h>
#include <utils/mobile_app_info.h>
#include <utils/common/app_info.h>
#include <common/common_module.h>
#include <context/connection_manager.h>
#include <context/context_settings.h>
#include <ui/window_utils.h>
#include <ui/texture_size_helper.h>
#include <ui/models/recent_user_connections_model.h>
#include <client_core/client_core_settings.h>
#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_app_info.h>
#include <watchers/available_cameras_watcher.h>
#include <watchers/cloud_status_watcher.h>

namespace {

const auto kUserRightsRefactoredVersion = QnSoftwareVersion(3, 0);

} // anonymous namespace

QnContext::QnContext(QObject* parent) :
    base_type(parent),
    m_connectionManager(new QnConnectionManager(this)),
    m_appInfo(new QnMobileAppInfo(this)),
    m_settings(new QnContextSettings(this))
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
    switch (static_cast<LiteModeType>(qnSettings->liteMode()))
    {
    case LiteModeType::LiteModeEnabled:
        return true;
    case LiteModeType::LiteModeAuto:
        return QnMobileClientAppInfo::defaultLiteMode();
    default:
        break;
    }
    return false;
}

void QnContext::removeSavedConnection(const QString& systemName)
{
    auto lastConnections = qnClientCoreSettings->recentUserConnections();

    auto connectionEqual = [systemName](const QnUserRecentConnectionData& connection)
    {
        return connection.systemName == systemName;
    };
    lastConnections.erase(std::remove_if(lastConnections.begin(), lastConnections.end(), connectionEqual),
                          lastConnections.end());

    qnClientCoreSettings->setRecentUserConnections(lastConnections);
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

    QnRecentUserConnectionsModel connectionsModel;
    connectionsModel.setSystemName(getLastUsedSystemId());
    if (!connectionsModel.hasConnections())
        return QString();

    const auto firstIndex = connectionsModel.index(0);
    QString password = connectionsModel.data(firstIndex, QnRecentUserConnectionsModel::PasswordRole).toString();
    if (password.isEmpty())
        return QString();

    url.setPassword(password);
    return url.toString();
}

void QnContext::setCloudCredentials(const QString& login, const QString& password)
{
    qnClientCoreSettings->setCloudLogin(login);
    qnClientCoreSettings->setCloudPassword(password);
    cloudStatusWatcher()->setCloudCredentials(login, password);
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
