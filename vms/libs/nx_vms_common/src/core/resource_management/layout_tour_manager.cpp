// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_tour_manager.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

using namespace nx::vms::api;

QnLayoutTourManager::QnLayoutTourManager(QObject* parent):
    base_type(parent)
{
}

QnLayoutTourManager::~QnLayoutTourManager()
{
}

const LayoutTourDataList& QnLayoutTourManager::tours() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_tours;
}

LayoutTourDataList QnLayoutTourManager::tours(const QList<QnUuid>& ids) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto matching = QSet(ids.begin(), ids.end());
    LayoutTourDataList result;
    for (const auto& tour: m_tours)
    {
        if (matching.contains(tour.id))
            result.push_back(tour);
    }
    return result;
}

void QnLayoutTourManager::resetTours(const LayoutTourDataList& tours)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    QHash<QnUuid, LayoutTourData> backup;
    for (auto tour: m_tours)
        backup.insert(tour.id, std::move(tour));
    m_tours = tours;
    lock.unlock();

    for (auto tour: tours)
    {
        auto old = backup.find(tour.id);
        if (old == backup.end())
        {
            emit tourAdded(tour);
        }
        else
        {
            if ((*old) != tour)
                emit tourChanged(tour);
            backup.erase(old);
        }
    }

    for (auto old: backup)
        emit tourRemoved(old.id);
}

LayoutTourData QnLayoutTourManager::tour(const QnUuid& id) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto iter = std::find_if(m_tours.cbegin(), m_tours.cend(),
        [&id](const LayoutTourData& data)
        {
            return data.id == id;
        });
    return iter != m_tours.cend() ? *iter : LayoutTourData();
}

void QnLayoutTourManager::addOrUpdateTour(const LayoutTourData& tour)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    for (auto& existing : m_tours)
    {
        if (existing.id == tour.id)
        {
            if (existing == tour)
                return;

            existing = tour;
            lock.unlock();

            emit tourChanged(tour);
            return;
        }
    }
    m_tours.push_back(tour);
    lock.unlock();

    emit tourAdded(tour);
}

void QnLayoutTourManager::removeTour(const QnUuid& tourId)
{
    if (tourId.isNull())
        NX_WARNING(this, "Removing tour with empty Id will not take effect");

    NX_MUTEX_LOCKER lock(&m_mutex);
    auto iter = std::find_if(m_tours.begin(), m_tours.end(),
        [tourId](const LayoutTourData& data)
        {
            return data.id == tourId;
        });

    if (iter == m_tours.end())
        return;

    m_tours.erase(iter);
    lock.unlock();

    emit tourRemoved(tourId);
}
