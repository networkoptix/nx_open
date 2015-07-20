#include "context.h"

#include "connection_manager.h"
#include "ui/color_theme.h"
#include "utils/mobile_app_info.h"
#include "camera/camera_thumbnail_cache.h"
#include "ui/resolution_util.h"
#include "login_session_manager.h"
#include "context_settings.h"
#include "ui/window_utils.h"

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
    connect(m_connectionManager, &QnConnectionManager::connectedChanged, this, &QnContext::at_connectionManager_connectedChanged);
}

QnContext::~QnContext() {}

int QnContext::dp(qreal dpix) const {
    return m_resolutionUtil->dp(dpix);
}

int QnContext::sp(qreal dpix) const {
    return m_resolutionUtil->sp(dpix);
}

void QnContext::exitFullscreen() {
    showSystemUi();
}

int QnContext::getStatusBarHeight() const {
    return statusBarHeight();
}

int QnContext::getNavigationBarHeight() const {
    return navigationBarHeight();
}

bool QnContext::getDeviceIsPhone() const {
    return !isTablet();
}

void QnContext::enterFullscreen() {
    hideSystemUi();
}

void QnContext::at_connectionManager_connectedChanged() {
    QnCameraThumbnailCache *thumbnailCache = QnCameraThumbnailCache::instance();
    if (!thumbnailCache)
        return;

    if (m_connectionManager->connected())
        thumbnailCache->start();
    else
        thumbnailCache->stop();
}
