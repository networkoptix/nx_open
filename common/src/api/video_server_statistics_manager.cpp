#include "video_server_statistics_manager.h"

#include <api/api_fwd.h>

#include <api/video_server_statistics_data.h>
#include <api/video_server_statistics_storage.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/video_server_resource.h>

QnVideoServerStatisticsManager::QnVideoServerStatisticsManager(QObject *parent):
    QObject(parent)
{}

qint64 QnVideoServerStatisticsManager::getHistory(QnVideoServerResourcePtr resource, qint64 lastId, QnStatisticsHistory *history){
    qDebug() << "history seek";
    QString id = resource->getUniqueId();

    if (!m_statistics.contains(id))
        m_statistics[id] = new QnStatisticsStorage(this);

    QnStatisticsStorage *storage = m_statistics[id];
    return storage->getHistory(lastId, history);
}

int QnVideoServerStatisticsManager::registerServerWidget(QnVideoServerResourcePtr resource, QObject *target, const char *slot){
    QString id = resource->getUniqueId();
    if (!m_statistics.contains(id))
        m_statistics[id] = new QnStatisticsStorage(this);

    QnStatisticsStorage *storage = m_statistics[id];
    return storage->registerServerWidget(target, slot);
}

void QnVideoServerStatisticsManager::unRegisterServerWidget(QnVideoServerResourcePtr resource, int widgetId, QObject *target){
    QString id = resource->getUniqueId();
    if (!m_statistics.contains(id))
        m_statistics[id] = new QnStatisticsStorage(this);

    QnStatisticsStorage *storage = m_statistics[id];
    storage->unRegisterServerWidget(widgetId, target);
}

void QnVideoServerStatisticsManager::notifyTimer(QnVideoServerResourcePtr resource, int widgetId){
    QString id = resource->getUniqueId();
    if (!m_statistics.contains(id))
        m_statistics[id] = new QnStatisticsStorage(this);
    QnStatisticsStorage *storage = m_statistics[id];
    storage->notifyTimer(resource->apiConnection(), widgetId);
}


