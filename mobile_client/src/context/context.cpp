#include "context.h"

#include "connection_manager.h"
#include "ui/color_theme.h"
#include "utils/mobile_app_info.h"
#include <utils/common/app_info.h>
#include "camera/camera_thumbnail_cache.h"
#include "ui/resolution_util.h"
#include "login_session_manager.h"
#include "context_settings.h"
#include "ui/window_utils.h"
#include "ui/texture_size_helper.h"
#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_app_info.h>

namespace {
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    const int customDensityClass = QnResolutionUtil::Mdpi;
#else
    const int customDensityClass = -1;
#endif
}

QnContext::QnContext(QObject *parent):
    base_type(parent),
    m_connectionManager(new QnConnectionManager(this)),
    m_loginSessionManager(new QnLoginSessionManager(this)),
    m_colorTheme(new QnColorTheme(this)),
    m_appInfo(new QnMobileAppInfo(this)),
    m_settings(new QnContextSettings(this)),
    m_resolutionUtil(customDensityClass == -1 ? new QnResolutionUtil() : new QnResolutionUtil(static_cast<QnResolutionUtil::DensityClass>(customDensityClass)))
{
    connect(m_connectionManager, &QnConnectionManager::connectionStateChanged, this, [this](){
        QnCameraThumbnailCache *thumbnailCache = QnCameraThumbnailCache::instance();
        if (!thumbnailCache)
            return;

        if (m_connectionManager->connectionState() == QnConnectionManager::Connected)
            thumbnailCache->start();
        else
            thumbnailCache->stop();
    });
}

QnContext::~QnContext() {}

int QnContext::dp(qreal dpix) const {
    return m_resolutionUtil->dp(dpix);
}

int QnContext::dps(qreal dpix) const {
    return m_resolutionUtil->dp(dpix) * m_resolutionUtil->pixelRatio();
}

int QnContext::sp(qreal dpix) const {
    return m_resolutionUtil->sp(dpix);
}

qreal QnContext::iconScale() const {
    return 1.0 / m_resolutionUtil->pixelRatio();
}

void QnContext::exitFullscreen() {
    showSystemUi();
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
