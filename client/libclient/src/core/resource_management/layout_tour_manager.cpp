#include "layout_tour_manager.h"

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>

#include <nx_ec/ec_api.h>

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

void QnLayoutTourManager::resetTours(const ec2::ApiLayoutTourDataList& tours)
{
    QnMutexLocker lock(&m_mutex);
    m_tours = tours;
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

void QnLayoutTourManager::saveTour(const ec2::ApiLayoutTourData& tour)
{
    NX_EXPECT(this->tour(tour.id).isValid());

    const auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;
    connection->getLayoutTourManager(Qn::kSystemAccess)->save(tour, this, 
        [](int /*reqId*/, ec2::ErrorCode /*errorCode*/) {});
}

void QnLayoutTourManager::removeTour(const ec2::ApiLayoutTourData& tour)
{
    QnMutexLocker lock(&m_mutex);
    auto iter = std::find_if(m_tours.begin(), m_tours.end(),
        [id = tour.id](const ec2::ApiLayoutTourData& data)
        {
            return data.id == id;
        });

    if (iter == m_tours.end())
        return;

    const QnUuid tourId = iter->id;
    NX_EXPECT(!tourId.isNull());

    m_tours.erase(iter);
    lock.unlock();

    const auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;
    connection->getLayoutTourManager(Qn::kSystemAccess)->remove(tourId, this, 
        [](int /*reqId*/, ec2::ErrorCode /*errorCode*/) {});

    emit tourRemoved(tour);
}
