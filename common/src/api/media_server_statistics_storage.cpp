#include "media_server_statistics_storage.h"

#include <api/media_server_statistics_data.h>
#include <core/resource/media_server_resource.h>

#include <utils/common/synctime.h>

/** Here can be any value below the zero */
#define NoData -1

QnMediaServerStatisticsStorage::QnMediaServerStatisticsStorage(const QnMediaServerConnectionPtr &apiConnection, int storageLimit, QObject *parent):
    QObject(parent),
    m_alreadyUpdating(false),
    m_lastId(-1),
    m_timeStamp(0),
    m_listeners(0),
    m_storageLimit(storageLimit),
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

QnStatisticsHistory QnMediaServerStatisticsStorage::history() const {
    return m_history;
}

qint64 QnMediaServerStatisticsStorage::historyId() const {
    if (m_history.isEmpty())
        return -1;
    return m_lastId;
}

void QnMediaServerStatisticsStorage::update() {
    if (!m_listeners || m_alreadyUpdating) {
        m_timeStamp = qnSyncTime->currentMSecsSinceEpoch();
        m_lastId++;

        for (QnStatisticsHistory::iterator iter = m_history.begin(); iter != m_history.end(); iter++) {
            QnStatisticsData &stats = iter.value();
            stats.values.append(NoData);
            if (stats.values.size() > m_storageLimit)
                stats.values.removeFirst();
        }
        if (!m_alreadyUpdating)
            return;
    }

    m_apiConnection->asyncGetStatistics(this, SLOT(at_statisticsReceived(int, const QnStatisticsDataList &, int)));
    m_alreadyUpdating = true;
}

void QnMediaServerStatisticsStorage::at_statisticsReceived(int status, const QnStatisticsDataList &data, int handle) {
    if(status != 0)
        return;

    m_timeStamp = qnSyncTime->currentMSecsSinceEpoch();
    m_lastId++;

    QSet<QString> notUpdated;
    foreach(QString key, m_history.keys())
        notUpdated << key;

    foreach(const QnStatisticsDataItem &nextData, data) {
        QString id = nextData.description;
        QnStatisticsData &stats = m_history[id];
        stats.deviceType = nextData.deviceType;
        stats.description = nextData.description;
        while (stats.values.count() < m_storageLimit)
            stats.values.append(NoData);
        stats.values.append(nextData.value);
        if (stats.values.size() > m_storageLimit)
            stats.values.removeFirst();
        notUpdated.remove(id);
    }

    foreach(QString id, notUpdated) {
        QnStatisticsData &stats = m_history[id];
        stats.values.append(NoData);
        if (stats.values.size() > m_storageLimit)
            stats.values.removeFirst();
        if (stats.values.count(NoData) == stats.values.count())
            m_history.remove(id);
    }

    m_alreadyUpdating = false;
    emit statisticsChanged();
}


