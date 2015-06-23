#include "context.h"

#include "connection_manager.h"
#include "ui/color_theme.h"
#include "utils/mobile_app_info.h"
#include "camera/camera_thumbnail_cache.h"
#include "ui/resolution_util.h"
#include "login_session_manager.h"

namespace {
#if 1
    const int custonDensityClass = QnResolutionUtil::Mdpi;
#else
    const int custonDensityClass = -1;
#endif
}

QnContext::QnContext(QObject *parent):
    base_type(parent),
    m_connectionManager(new QnConnectionManager(this)),
    m_loginSessionManager(new QnLoginSessionManager(this)),
    m_colorTheme(new QnColorTheme(this)),
    m_appInfo(new QnMobileAppInfo(this)),
    m_resolutionUtil(custonDensityClass == -1 ? new QnResolutionUtil() : new QnResolutionUtil(static_cast<QnResolutionUtil::DensityClass>(custonDensityClass)))
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

void QnContext::at_connectionManager_connectedChanged() {
    QnCameraThumbnailCache *thumbnailCache = QnCameraThumbnailCache::instance();
    if (!thumbnailCache)
        return;

    if (m_connectionManager->connected())
        thumbnailCache->start();
    else
        thumbnailCache->stop();
}
