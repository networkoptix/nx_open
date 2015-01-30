#include "context.h"

#include "connection_manager.h"
#include "ui/color_theme.h"
#include "utils/mobile_app_info.h"
#include "camera/camera_thumbnail_cache.h"

QnContext::QnContext(QObject *parent):
    base_type(parent),
    m_connectionManager(new QnConnectionManager(this)),
    m_colorTheme(new QnColorTheme(this)),
    m_appInfo(new QnMobileAppInfo(this))
{
    connect(m_connectionManager,    &QnConnectionManager::connected,    this,   &QnContext::at_connectionManager_connected);
    connect(m_connectionManager,    &QnConnectionManager::disconnected, this,   &QnContext::at_connectionManager_disconnected);
}

QnContext::~QnContext() {}

void QnContext::at_connectionManager_connected() {
    if (QnCameraThumbnailCache::instance())
        QnCameraThumbnailCache::instance()->start();
}

void QnContext::at_connectionManager_disconnected() {
    if (QnCameraThumbnailCache::instance())
        QnCameraThumbnailCache::instance()->stop();
}
