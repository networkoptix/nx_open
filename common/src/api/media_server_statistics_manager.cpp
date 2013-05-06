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

void QnMediaServerStatisticsManager::registerServerWidget(const QnMediaServerResourcePtr &resource, QObject *target, const char *slot){
    QString id = resource->getGuid();
    if (!m_statistics.contains(id))
        m_statistics[id] = new QnMediaServerStatisticsStorage(resource->apiConnection(), pointsLimit(), this);
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

int QnMediaServerStatisticsManager::updatePeriod(const QnMediaServerResourcePtr &resource) const {
    QString id = resource->getGuid();
    if (!m_statistics.contains(id))
        return -1;
    return m_statistics[id]->updatePeriod();
}

int QnMediaServerStatisticsManager::pointsLimit() const {
    return defaultPointsLimit;
}
