#include "video_server_statistics_manager.h"

#include <api/video_server_statistics_data.h>
#include <api/video_server_connection.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/video_server_resource.h>

#include <utils/common/synctime.h>

#define STORAGE_LIMIT 60
#define REQUEST_TIME 2000

QnStatisticsStorage::QnStatisticsStorage(QObject *parent):
    QObject(parent),
    m_alreadyUpdating(false),
    m_lastId(-1),
    m_timeStamp(0)
{
}

qint64 QnStatisticsStorage::getHistory(qint64 lastId, QnStatisticsHistory *history){
    if (m_history.isEmpty())
        return -1;

    QnStatisticsIterator iter(m_history);
    while (iter.hasNext()){
        iter.next();
        QnStatisticsData* stats = new QnStatisticsData();
        history->insert(iter.key(), stats);

        qint64 counter = m_lastId;
        QnStatisticsDataIterator peeker(*iter.value());
        peeker.toBack();
        while (counter > lastId && peeker.hasPrevious()){
            stats->prepend(peeker.previous());
            counter--;
        }
    }

    return m_lastId;
}

void QnStatisticsStorage::update(QnVideoServerConnectionPtr apiConnection){
    qDebug() << "trying to update";
    if (qnSyncTime->currentMSecsSinceEpoch() - m_timeStamp < REQUEST_TIME)
        return;

    qDebug() << "successfull request";
    apiConnection->asyncGetStatistics(this, SLOT(at_statisticsReceived(const QnStatisticsDataList &)));
    m_alreadyUpdating = true;
}

void QnStatisticsStorage::at_statisticsReceived(const QnStatisticsDataList &data){
    qDebug() << "responce received";
    int cpuCounter = 0;
    int hddCounter = 0;
    m_timeStamp = qnSyncTime->currentMSecsSinceEpoch();
    m_lastId++;

    QListIterator<QnStatisticsDataItem> iter(data);
    while(iter.hasNext()){
        QnStatisticsDataItem next_data = iter.next();

        QString id = next_data.device == QnStatisticsDataItem::CPU
           ? tr("CPU") + QString::number(cpuCounter++)
           : tr("HDD") + QString::number(hddCounter++);

        if (!m_history.contains(id))
            m_history[id] = new QnStatisticsData();

        QnStatisticsData *stats = m_history[id];
        stats->append(next_data.value);
        if (stats->size() > STORAGE_LIMIT)
            stats->removeFirst();
    }
    m_alreadyUpdating = false;
}

qint64 QnVideoServerStatisticsManager::getHistory(QnVideoServerResourcePtr resource, qint64 lastId, QnStatisticsHistory *history){
    qDebug() << "history seek";
    QString id = resource->getUniqueId();

    if (!m_statistics.contains(id))
        m_statistics[id] = new QnStatisticsStorage(this);

    QnStatisticsStorage *storage = m_statistics[id];
    if (storage->isAlreadyUpdating())
        return -1;

    storage->update(resource->apiConnection());
    return storage->getHistory(lastId, history);

}
