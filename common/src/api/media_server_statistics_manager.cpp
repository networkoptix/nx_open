#include "media_server_statistics_manager.h"

#include <QtCore/QTimer>

#include <api/media_server_statistics_data.h>
#include <api/media_server_statistics_storage.h>

#include <core/resource/media_server_resource.h>

/** Data update period. For the best result should be equal to server's */
#define REQUEST_TIME 2000 //TODO: #GDM user server's value from xml

QnMediaServerStatisticsManager::QnMediaServerStatisticsManager(QObject *parent):
    QObject(parent)
{
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(at_timer_timeout()));
    timer->start(REQUEST_TIME);
}

void QnMediaServerStatisticsManager::registerServerWidget(const QnMediaServerResourcePtr &resource, QObject *target, const char *slot){
    QString id = resource->getGuid();
    if (!m_statistics.contains(id))
        m_statistics[id] = new QnMediaServerStatisticsStorage(resource->apiConnection(), storageLimit(), this);
    m_statistics[id]->registerServerWidget(target, slot);
}

void QnMediaServerStatisticsManager::unregisterServerWidget(const QnMediaServerResourcePtr &resource, QObject *target){
    QString id = resource->getGuid();
    if (!m_statistics.contains(id))
        return;
    m_statistics[id]->unregisterServerWidget(target);
}

QnStatisticsHistory QnMediaServerStatisticsManager::history(const QnMediaServerResourcePtr &resource) const {
    QString id = resource->getGuid();
    if (!m_statistics.contains(id))
        return QnStatisticsHistory();
    return m_statistics[id]->history();
}

qint64 QnMediaServerStatisticsManager::historyId(const QnMediaServerResourcePtr &resource) const {
    QString id = resource->getGuid();
    if (!m_statistics.contains(id))
        return -1;
    return m_statistics[id]->historyId();
}

void QnMediaServerStatisticsManager::at_timer_timeout(){
    foreach(QnMediaServerStatisticsStorage *storage, m_statistics)
        storage->update();
}
