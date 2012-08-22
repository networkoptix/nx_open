#include "video_server_statistics_storage.h"

#include <api/video_server_statistics_data.h>
#include <api/video_server_connection.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/video_server_resource.h>

#include <utils/common/synctime.h>

#define STORAGE_LIMIT 60

QnStatisticsStorage::QnStatisticsStorage(QObject *parent):
    QObject(parent),
    m_alreadyUpdating(false),
    m_lastId(-1),
    m_timeStamp(0),
    m_activeWidget(-1),
    m_lastWidget(-1)
{
}

qint64 QnStatisticsStorage::getHistory(qint64 lastId, QnStatisticsHistory *history){
    qDebug() << "requested history from" << lastId << "to" << m_lastId;

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
    apiConnection->asyncGetStatistics(this, SLOT(at_statisticsReceived(const QnStatisticsDataList &)));
    m_alreadyUpdating = true;
}

void QnStatisticsStorage::at_statisticsReceived(const QnStatisticsDataList &data){
    qDebug() << "statistics received";
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

    emit at_statistics_processed();
}

int QnStatisticsStorage::registerServerWidget(QObject *target, const char *slot){
    connect(this, SIGNAL(at_statistics_processed()), target, slot);

    m_lastWidget++;

    qDebug() << "widget" << m_lastWidget << "registered";
    return m_lastWidget;
}

void QnStatisticsStorage::unRegisterServerWidget(int widgetId, QObject *target){
    qDebug() << "widget" << widgetId << "unregistered";

    if (m_activeWidget == widgetId){
        m_activeWidget = -1;
        qDebug() << "active widget cleared";
    }
    disconnect(this, 0, target, 0);
}

void QnStatisticsStorage::notifyTimer(QnVideoServerConnectionPtr apiConnection, int widgetId){
    qDebug() << "notify timer " << widgetId;
    if (m_activeWidget < 0){
        m_activeWidget = widgetId;
        qDebug() << "widget" << widgetId << "set as active";
    }

    if (m_activeWidget != widgetId)
        return;

    qDebug() << "widget" << widgetId << "initiated update";
    update(apiConnection);
}
