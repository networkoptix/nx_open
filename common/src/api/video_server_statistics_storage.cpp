#include "video_server_statistics_storage.h"

#include <api/video_server_statistics_data.h>
#include <core/resource/video_server_resource.h>

#include <utils/common/synctime.h>

#define STORAGE_LIMIT 60

QnStatisticsStorage::QnStatisticsStorage(const QnVideoServerConnectionPtr &apiConnection, QObject *parent):
    QObject(parent),
    m_alreadyUpdating(false),
    m_lastId(-1),
    m_timeStamp(0),
    m_listeners(0),
    m_apiConnection(apiConnection)
{}

void QnStatisticsStorage::registerServerWidget(QObject *target, const char *slot) {
    connect(this, SIGNAL(statisticsChanged()), target, slot);
    m_listeners++;
}

void QnStatisticsStorage::unregisterServerWidget(QObject *target) {
    disconnect(this, 0, target, 0);
    m_listeners--;
}

qint64 QnStatisticsStorage::getHistory(qint64 lastId, QnStatisticsHistory *history) {
    if (m_history.isEmpty())
        return -1;

    QnStatisticsIterator iter(m_history);
    while (iter.hasNext()){
        iter.next();
        QnStatisticsData stats;
        qint64 counter = m_lastId;
        QnStatisticsDataIterator peeker(iter.value());
        peeker.toBack();
        while (counter > lastId && peeker.hasPrevious()) {
            stats.prepend(peeker.previous());
            counter--;
        }
        history->insert(iter.key(), stats);
    }

    return m_lastId;
}

void QnStatisticsStorage::update() {
    if (m_alreadyUpdating)
        return;

    if (!m_listeners) {
        m_timeStamp = qnSyncTime->currentMSecsSinceEpoch();
        m_lastId++;

        for (QnStatisticsHistory::iterator iter = m_history.begin(); iter != m_history.end(); iter++) {
            QnStatisticsData &stats = iter.value();
            stats.append(0);
            if (stats.size() > STORAGE_LIMIT)
                stats.removeFirst();
        }
        return;
    }

    m_apiConnection->asyncGetStatistics(this, SLOT(at_statisticsReceived(const QnStatisticsDataList &)));
    m_alreadyUpdating = true;
}

void QnStatisticsStorage::at_statisticsReceived(const QnStatisticsDataList &data) {
    m_timeStamp = qnSyncTime->currentMSecsSinceEpoch();
    m_lastId++;

    QListIterator<QnStatisticsDataItem> iter(data);
    while(iter.hasNext()) {
        QnStatisticsDataItem nextData = iter.next();

        QString id = nextData.deviceType == QnStatisticsDataItem::CPU
           ? tr("CPU")
           : nextData.description;

        QnStatisticsData &stats = m_history[id];
        stats.append(nextData.value);
        if (stats.size() > STORAGE_LIMIT)
            stats.removeFirst();
    }
    m_alreadyUpdating = false;

    emit statisticsChanged();
}


