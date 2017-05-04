#include "layout_tour_access_provider.h"

#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>

#include <core/resource/resource.h>

QnLayoutTourAccessProvider::QnLayoutTourAccessProvider(QObject* parent):
    base_type(parent)
{
    auto updateAccessByTourId = [this](const QnUuid& tourId)
        {
            for (const auto& layout: resourcePool()->getResourcesWithParentId(tourId))
                updateAccessToResource(layout);
        };
    auto updateAccessByTour = [updateAccessByTourId](const ec2::ApiLayoutTourData& tour)
        {
            updateAccessByTourId(tour.id);
        };

    connect(layoutTourManager(), &QnLayoutTourManager::tourAdded, this, updateAccessByTour);
    //connect(layoutTourManager(), &QnLayoutTourManager::tourChanged, this, updateAccessByTour);
    connect(layoutTourManager(), &QnLayoutTourManager::tourRemoved, this, updateAccessByTourId);
}

QnLayoutTourAccessProvider::~QnLayoutTourAccessProvider()
{
}

QnAbstractResourceAccessProvider::Source QnLayoutTourAccessProvider::baseSource() const
{
    return Source::tour;
}

bool QnLayoutTourAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!resource->hasFlags(Qn::layout))
        return false;

    const auto parentId = resource->getParentId();
    if (parentId.isNull())
        return false;

    const auto tour = layoutTourManager()->tour(parentId);
    if (!tour.isValid())
        return false;

    if (tour.parentId != subject.id())
        return false;

    return true;
}
