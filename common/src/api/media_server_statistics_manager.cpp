#include "media_server_statistics_manager.h"

#include <QtCore/QTimer>

#include <api/media_server_statistics_storage.h>
#include <core/resource/media_server_resource.h>

namespace {
    const int defaultPointsLimit = 60;
}

QnMediaServerStatisticsManager::QnMediaServerStatisticsManager(QObject *parent):
    QObject(parent)
{}

void QnMediaServerStatisticsManager::registerConsumer(const QnMediaServerResourcePtr &resource, QObject *target, const char *slot){
    QString id = resource->getGuid();
    if (!m_statistics.contains(id)) {
        m_statistics[id] = new QnMediaServerStatisticsStorage(resource->apiConnection(), pointsLimit(), this);
        foreach (QnStatisticsDeviceType key, m_flagsFilter.keys())
            m_statistics[id]->setFlagsFilter(key, m_flagsFilter[key]);
    }
    m_statistics[id]->registerConsumer(target, slot);
}

void QnMediaServerStatisticsManager::unregisterConsumer(const QnMediaServerResourcePtr &resource, QObject *target){
    QString id = resource->getGuid();
    if (!m_statistics.contains(id))
        return;
    m_statistics[id]->unregisterConsumer(target);
}

QnStatisticsHistory QnMediaServerStatisticsManager::history(const QnMediaServerResourcePtr &resource) const {
    QString id = resource->getGuid();
    if (!m_statistics.contains(id))
        return QnStatisticsHistory();
    return m_statistics[id]->history();
}

qint64 QnMediaServerStatisticsManager::uptimeMs(const QnMediaServerResourcePtr &resource) const
{
    QString id = resource->getGuid();
    if (!m_statistics.contains(id))
        return 0;
    return m_statistics[id]->uptimeMs();
}

qint64 QnMediaServerStatisticsManager::historyId(const QnMediaServerResourcePtr &resource) const {
    QString id = resource->getGuid();
    if (!m_statistics.contains(id))
        return -1;
    return m_statistics[id]->historyId();
}

int QnMediaServerStatisticsManager::updatePeriod(const QnMediaServerResourcePtr &resource) const {
    QString id = resource->getGuid();
    if (!m_statistics.contains(id))
        return -1;
    return m_statistics[id]->updatePeriod();
}

int QnMediaServerStatisticsManager::pointsLimit() const {
    return defaultPointsLimit;
}

void QnMediaServerStatisticsManager::setFlagsFilter(QnStatisticsDeviceType deviceType, int flags) {
    m_flagsFilter[deviceType] = flags;
    foreach(QnMediaServerStatisticsStorage* storage, m_statistics)
        storage->setFlagsFilter(deviceType, flags);
}
