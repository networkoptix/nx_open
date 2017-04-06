#include "layout_tour_manager.h"

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>

#include <nx_ec/data/api_layout_tour_data.h>

QnLayoutTourManager::QnLayoutTourManager(QObject* parent)
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

const ec2::ApiLayoutTourData& QnLayoutTourManager::tour(const QnUuid& id) const
{
    QnMutexLocker lock(&m_mutex);
    auto iter = std::find_if(m_tours.cbegin(), m_tours.cend(),
        [&id](const ec2::ApiLayoutTourData& data)
        {
            return data.id == id;
        });
    return iter != m_tours.cend() ? *iter : ec2::ApiLayoutTourData();
}

QnLayoutTourItemList QnLayoutTourManager::tourItems(const QnUuid& id) const
{
    return tourItems(tour(id));
}

QnLayoutTourItemList QnLayoutTourManager::tourItems(const ec2::ApiLayoutTourData& tour) const
{
    QnLayoutTourItemList result;
    result.reserve(tour.items.size());
    for (const auto& item: tour.items)
    {
        if (const auto& layout = qnResPool->getResourceById<QnLayoutResource>(item.layoutId))
            result.emplace_back(layout, item.delayMs);
    }
    return result;
}


//TODO: #GDM #3.1 #high think about semantics: who will save on server and who update locally
void QnLayoutTourManager::saveTour(const ec2::ApiLayoutTourData& tour)
{
    for (auto& existing: m_tours)
    {
        if (existing.id == tour.id)
        {
            existing = tour;
            return;
        }
    }
    m_tours.push_back(tour);

//     const auto connection = QnAppServerConnectionFactory::getConnection2();
//     if (!connection)
//         return;
//     connection->getLayoutTourManager(Qn::kSystemAccess)->save(tour, this, nullptr);
}
