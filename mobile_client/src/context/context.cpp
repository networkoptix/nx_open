#include "context.h"

#include "connection_manager.h"
#include "ui/color_theme.h"
#include "utils/mobile_app_info.h"
#include "camera/camera_thumbnail_cache.h"
#include "ui/resolution_util.h"

QnContext::QnContext(QObject *parent):
    base_type(parent),
    m_connectionManager(new QnConnectionManager(this)),
    m_colorTheme(new QnColorTheme(this)),
    m_appInfo(new QnMobileAppInfo(this)),
    m_resolutionUtil(new QnResolutionUtil())
{
    connect(m_connectionManager,    &QnConnectionManager::connected,    this,   &QnContext::at_connectionManager_connected);
    connect(m_connectionManager,    &QnConnectionManager::disconnected, this,   &QnContext::at_connectionManager_disconnected);
}

QnContext::~QnContext() {}

int QnContext::dp(qreal dpix) const {
    return m_resolutionUtil->dp(dpix);
}

int QnContext::sp(qreal dpix) const {
    return m_resolutionUtil->sp(dpix);
}

void QnContext::at_connectionManager_connected() {
    if (QnCameraThumbnailCache::instance())
        QnCameraThumbnailCache::instance()->start();
}

void QnContext::at_connectionManager_disconnected() {
    if (QnCameraThumbnailCache::instance())
        QnCameraThumbnailCache::instance()->stop();
}
