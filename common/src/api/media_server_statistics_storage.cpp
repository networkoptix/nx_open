#include "media_server_statistics_storage.h"

#include <QtCore/QTimer>

#include <core/resource/media_server_resource.h>

#include <utils/common/synctime.h>

namespace {
    const int noDataValue = -1;
    const int defaultUpdatePeriod = 500;

    /**
     * Set size of values to pointsLimit. Fill by noDataValue or cut from the beginning if needed.
     */
    void normalizeValuesList(QLinkedList<qreal> &values, int pointsLimit) {
        while (values.size() < pointsLimit)
            values.prepend(noDataValue);
        while (values.size() > pointsLimit)
            values.removeFirst();
    }
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
    m_timer(new QTimer()),
    m_uptimeMs(0)
{
    connect(m_timer, SIGNAL(timeout()), this, SLOT(update()));
    m_timer->start(m_updatePeriod);
}

void QnMediaServerStatisticsStorage::registerConsumer(QObject *target, const char *slot) {
    connect(this, SIGNAL(statisticsChanged()), target, slot);
    m_listeners++;
}

void QnMediaServerStatisticsStorage::unregisterConsumer(QObject *target) {
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

qint64 QnMediaServerStatisticsStorage::uptimeMs() const
{
    return m_uptimeMs;
}

void QnMediaServerStatisticsStorage::setFlagsFilter(QnStatisticsDeviceType deviceType, int flags) {
    m_flagsFilter[deviceType] = flags;
}

void QnMediaServerStatisticsStorage::update() {
    if (!m_listeners || m_alreadyUpdating) {
        m_timeStamp = qnSyncTime->currentMSecsSinceEpoch();
        m_lastId++;

        for (QnStatisticsHistory::iterator iter = m_history.begin(); iter != m_history.end(); iter++) {
            QnStatisticsData &stats = iter.value();
            stats.values.append(noDataValue);
            normalizeValuesList(stats.values, m_pointsLimit);
        }
    }

    emit statisticsChanged();

    if (!m_listeners || m_alreadyUpdating)
        return;

    m_apiConnection->getStatisticsAsync(this, SLOT(at_statisticsReceived(int, const QnStatisticsReply &, int)));

    m_alreadyUpdating = true;
}

void QnMediaServerStatisticsStorage::at_statisticsReceived(int status, const QnStatisticsReply &reply, int handle) {
    Q_UNUSED(handle)

    m_alreadyUpdating = false;
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

    m_uptimeMs = reply.uptimeMs;

    foreach(const QnStatisticsDataItem &nextData, reply.statistics) 
    {

        if (m_flagsFilter.contains(nextData.deviceType) &&
            ((m_flagsFilter[nextData.deviceType] & nextData.deviceFlags) != m_flagsFilter[nextData.deviceType]))
            continue;

        QString id = nextData.description;
        notUpdated.remove(id);

        QnStatisticsData &stats = m_history[id];
        stats.deviceType = nextData.deviceType;
        stats.description = nextData.description;
        stats.values.append(nextData.value);
        normalizeValuesList(stats.values, m_pointsLimit);
    }

    foreach(QString id, notUpdated) {
        QnStatisticsData &stats = m_history[id];
        stats.values.append(noDataValue);
        normalizeValuesList(stats.values, m_pointsLimit);
        if (stats.values.count(noDataValue) == stats.values.size())
            m_history.remove(id);
    }


//    emit statisticsChanged();
}

