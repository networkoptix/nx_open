#include "media_server_statistics_manager.h"

#include <QtCore/QTimer>

#include <api/media_server_statistics_data.h>
#include <api/media_server_statistics_storage.h>

#include <core/resource/media_server_resource.h>

/** Data update period. For the best result should be equal to server's */
#define REQUEST_TIME 2000

QnMediaServerStatisticsManager::QnMediaServerStatisticsManager(QObject *parent):
    QObject(parent)
{
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(at_timer_timeout()));
    timer->start(REQUEST_TIME);
}

void QnMediaServerStatisticsManager::registerServerWidget(const QnMediaServerResourcePtr &resource, QObject *target, const char *slot){
    QString id = resource->getUniqueId();
    if (!m_statistics.contains(id))
        m_statistics[id] = new QnMediaServerStatisticsStorage(resource->apiConnection(), this);
    m_statistics[id]->registerServerWidget(target, slot);
}

void QnMediaServerStatisticsManager::unregisterServerWidget(const QnMediaServerResourcePtr &resource, QObject *target){
    QString id = resource->getUniqueId();
    if (!m_statistics.contains(id))
        return;
    m_statistics[id]->unregisterServerWidget(target);
}

qint64 QnMediaServerStatisticsManager::getHistory(const QnMediaServerResourcePtr &resource, qint64 lastId, QnStatisticsHistory *history){
    QString id = resource->getUniqueId();
    if (!m_statistics.contains(id))
        return -1;
    return m_statistics[id]->getHistory(lastId, history);
}

void QnMediaServerStatisticsManager::at_timer_timeout(){
    foreach(QnMediaServerStatisticsStorage *storage, m_statistics)
        storage->update();
}
