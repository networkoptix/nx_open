#include "media_server_statistics_storage.h"

#include <QtCore/QTimer>

#include <core/resource/media_server_resource.h>

#include <utils/common/synctime.h>

namespace {
    const int noDataValue = -1;
    const int defaultUpdatePeriod = 500;
}

QnMediaServerStatisticsStorage::QnMediaServerStatisticsStorage(const QnMediaServerConnectionPtr &apiConnection, int pointsLimit, QObject *parent):
    QObject(parent),
    m_alreadyUpdating(false),
    m_lastId(-1),
    m_timeStamp(0),
    m_listeners(0),
    m_pointsLimit(pointsLimit),
    m_updatePeriod(defaultUpdatePeriod),
    m_apiConnection(apiConnection),
    m_timer(new QTimer())
{
    connect(m_timer, SIGNAL(timeout()), this, SLOT(update()));
    m_timer->start(m_updatePeriod);
}

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

int QnMediaServerStatisticsStorage::updatePeriod() const {
    return m_updatePeriod;
}

void QnMediaServerStatisticsStorage::update() {
    if (!m_listeners || m_alreadyUpdating) {
        m_timeStamp = qnSyncTime->currentMSecsSinceEpoch();
        m_lastId++;

        for (QnStatisticsHistory::iterator iter = m_history.begin(); iter != m_history.end(); iter++) {
            QnStatisticsData &stats = iter.value();
            stats.values.append(noDataValue);
            if (stats.values.size() > m_pointsLimit)
                stats.values.removeFirst();
        }
        if (!m_alreadyUpdating)
            return;
    }

    m_apiConnection->asyncGetStatistics(this, SLOT(at_statisticsReceived(int, const QnStatisticsReply &, int)));
    m_alreadyUpdating = true;
}

void QnMediaServerStatisticsStorage::at_statisticsReceived(int status, const QnStatisticsReply &reply, int handle) {
    Q_UNUSED(handle)
    if(status != 0)
        return;

    m_timeStamp = qnSyncTime->currentMSecsSinceEpoch();
    m_lastId++;

    if (reply.updatePeriod > 0 && m_updatePeriod != reply.updatePeriod) {
        m_updatePeriod = reply.updatePeriod;
        m_timer->stop();
        m_timer->start(m_updatePeriod);
    }

    QSet<QString> notUpdated;
    foreach(QString key, m_history.keys())
        notUpdated << key;

    foreach(const QnStatisticsDataItem &nextData, reply.statistics) {
        QString id = nextData.description;
        QnStatisticsData &stats = m_history[id];
        stats.deviceType = nextData.deviceType;
        stats.description = nextData.description;
        while (stats.values.count() < m_pointsLimit)
            stats.values.append(noDataValue);
        stats.values.append(nextData.value);
        if (stats.values.size() > m_pointsLimit)
            stats.values.removeFirst();
        notUpdated.remove(id);
    }

    foreach(QString id, notUpdated) {
        QnStatisticsData &stats = m_history[id];
        stats.values.append(noDataValue);
        if (stats.values.size() > m_pointsLimit)
            stats.values.removeFirst();
        if (stats.values.count(noDataValue) == stats.values.count())
            m_history.remove(id);
    }

    m_alreadyUpdating = false;
    emit statisticsChanged();
}


