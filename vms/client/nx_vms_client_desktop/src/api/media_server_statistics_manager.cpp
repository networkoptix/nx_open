// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_server_statistics_manager.h"

#include <QtCore/QTimer>

#include <api/media_server_statistics_storage.h>
#include <core/resource/media_server_resource.h>
#include <nx/vms/client/desktop/system_context.h>

using namespace nx::vms::client::desktop;

QnMediaServerStatisticsManager::QnMediaServerStatisticsManager(
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext)
{
}

void QnMediaServerStatisticsManager::registerConsumer(
    const QnMediaServerResourcePtr& resource,
    QObject* target,
    std::function<void()> callback)
{
    if (!NX_ASSERT(resource->systemContext() == systemContext()))
        return;

    QnUuid id = resource->getId();
    if (!m_statistics.contains(id))
    {
        m_statistics[id] = new QnMediaServerStatisticsStorage(
            systemContext(),
            id,
            kPointsLimit,
            this);

        for (Qn::StatisticsDeviceType key: m_flagsFilter.keys())
            m_statistics[id]->setFlagsFilter(key, m_flagsFilter[key]);
    }
    m_statistics[id]->registerConsumer(target, callback);
}

void QnMediaServerStatisticsManager::unregisterConsumer(
    const QnMediaServerResourcePtr& resource,
    QObject* target)
{
    NX_ASSERT(resource->systemContext() == systemContext());

    QnUuid id = resource->getId();
    if (!m_statistics.contains(id))
        return;

    m_statistics[id]->unregisterConsumer(target);
}

QnStatisticsHistory QnMediaServerStatisticsManager::history(
    const QnMediaServerResourcePtr& resource) const
{
    QnUuid id = resource->getId();
    if (!m_statistics.contains(id))
        return QnStatisticsHistory();

    return m_statistics[id]->history();
}

qint64 QnMediaServerStatisticsManager::uptimeMs(const QnMediaServerResourcePtr& resource) const
{
    QnUuid id = resource->getId();
    if (!m_statistics.contains(id))
        return 0;

    return m_statistics[id]->uptimeMs();
}

qint64 QnMediaServerStatisticsManager::historyId(const QnMediaServerResourcePtr& resource) const
{
    QnUuid id = resource->getId();
    if (!m_statistics.contains(id))
        return -1;

    return m_statistics[id]->historyId();
}

int QnMediaServerStatisticsManager::updatePeriod(const QnMediaServerResourcePtr& resource) const
{
    QnUuid id = resource->getId();
    if (!m_statistics.contains(id))
        return -1;

    return m_statistics[id]->updatePeriod();
}

void QnMediaServerStatisticsManager::setFlagsFilter(Qn::StatisticsDeviceType deviceType, int flags)
{
    m_flagsFilter[deviceType] = flags;
    for (QnMediaServerStatisticsStorage* storage: m_statistics)
        storage->setFlagsFilter(deviceType, flags);
}
