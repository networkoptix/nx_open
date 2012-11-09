#include "media_server_statistics_storage.h"

#include <api/media_server_statistics_data.h>
#include <core/resource/media_server_resource.h>

#include <utils/common/synctime.h>

#define STORAGE_LIMIT 60

QnMediaServerStatisticsStorage::QnMediaServerStatisticsStorage(const QnMediaServerConnectionPtr &apiConnection, QObject *parent):
    QObject(parent),
    m_alreadyUpdating(false),
    m_lastId(-1),
    m_timeStamp(0),
    m_listeners(0),
    m_apiConnection(apiConnection)
{}

void QnMediaServerStatisticsStorage::registerServerWidget(QObject *target, const char *slot) {
    connect(this, SIGNAL(statisticsChanged()), target, slot);
    m_listeners++;
}

void QnMediaServerStatisticsStorage::unregisterServerWidget(QObject *target) {
    disconnect(this, 0, target, 0);
    m_listeners--;
}

qint64 QnMediaServerStatisticsStorage::getHistory(qint64 lastId, QnStatisticsHistory *history) {
    if (m_history.isEmpty())
        return -1;

    QnStatisticsIterator iter(m_history);
    while (iter.hasNext()){
        iter.next();
        QnStatisticsData item = iter.value();
        QnStatisticsData update;
        update.description = item.description;
        update.deviceType = item.deviceType;
        qint64 counter = m_lastId;
        QnStatisticsDataIterator peeker(item.values);
        peeker.toBack();
        while (counter > lastId && peeker.hasPrevious()) {
            update.values.prepend(peeker.previous());
            counter--;
        }
        history->insert(iter.key(), update);
    }

    return m_lastId;
}

void QnMediaServerStatisticsStorage::update() {
    if (!m_listeners || m_alreadyUpdating) {
        m_timeStamp = qnSyncTime->currentMSecsSinceEpoch();
        m_lastId++;

        for (QnStatisticsHistory::iterator iter = m_history.begin(); iter != m_history.end(); iter++) {
            QnStatisticsData &stats = iter.value();
            stats.values.append(0);
            if (stats.values.size() > STORAGE_LIMIT)
                stats.values.removeFirst();
        }
        if (!m_alreadyUpdating)
            return;
    }

    m_apiConnection->asyncGetStatistics(this, SLOT(at_statisticsReceived(const QnStatisticsDataList &)));
    m_alreadyUpdating = true;
}

void QnMediaServerStatisticsStorage::at_statisticsReceived(const QnStatisticsDataList &data) {
    m_timeStamp = qnSyncTime->currentMSecsSinceEpoch();
    m_lastId++;

    QListIterator<QnStatisticsDataItem> iter(data);
    while(iter.hasNext()) {
        QnStatisticsDataItem nextData = iter.next();

        QString id = nextData.description;
        QnStatisticsData &stats = m_history[id];
        stats.deviceType = nextData.deviceType;
        stats.description = nextData.description;
        stats.values.append(nextData.value);
        if (stats.values.size() > STORAGE_LIMIT)
            stats.values.removeFirst();
    }
    m_alreadyUpdating = false;

    emit statisticsChanged();
}


