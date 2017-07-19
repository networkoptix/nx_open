#include "layout_tour_manager.h"

#include <nx/utils/log/assert.h>

QnLayoutTourManager::QnLayoutTourManager(QObject* parent):
    base_type(parent)
{
}

QnLayoutTourManager::~QnLayoutTourManager()
{
}

const ec2::ApiLayoutTourDataList& QnLayoutTourManager::tours() const
{
    QnMutexLocker lock(&m_mutex);
    return m_tours;
}

ec2::ApiLayoutTourDataList QnLayoutTourManager::tours(const QList<QnUuid>& ids) const
{
    QnMutexLocker lock(&m_mutex);

    auto matching = ids.toSet();
    ec2::ApiLayoutTourDataList result;
    for (const auto& tour: m_tours)
    {
        if (matching.contains(tour.id))
            result.push_back(tour);
    }
    return result;
}

void QnLayoutTourManager::resetTours(const ec2::ApiLayoutTourDataList& tours)
{
    QnMutexLocker lock(&m_mutex);
    QHash<QnUuid, ec2::ApiLayoutTourData> backup;
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

ec2::ApiLayoutTourData QnLayoutTourManager::tour(const QnUuid& id) const
{
    QnMutexLocker lock(&m_mutex);
    auto iter = std::find_if(m_tours.cbegin(), m_tours.cend(),
        [&id](const ec2::ApiLayoutTourData& data)
        {
            return data.id == id;
        });
    return iter != m_tours.cend() ? *iter : ec2::ApiLayoutTourData();
}

void QnLayoutTourManager::addOrUpdateTour(const ec2::ApiLayoutTourData& tour)
{
    QnMutexLocker lock(&m_mutex);
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
    NX_EXPECT(!tourId.isNull());

    QnMutexLocker lock(&m_mutex);
    auto iter = std::find_if(m_tours.begin(), m_tours.end(),
        [tourId](const ec2::ApiLayoutTourData& data)
        {
            return data.id == tourId;
        });

    if (iter == m_tours.end())
        return;

    m_tours.erase(iter);
    lock.unlock();

    emit tourRemoved(tourId);
}
