#include "motion_search_list_model.h"
#include "private/motion_search_list_model_p.h"

#include <algorithm>

namespace nx::vms::client::desktop {

MotionSearchListModel::MotionSearchListModel(QnWorkbenchContext* context, QObject* parent):
    base_type(context, [this]() { return new Private(this); }, parent),
    d(qobject_cast<Private*>(base_type::d.data()))
{
    setLiveSupported(true);
    setLivePaused(true);
}

QList<QRegion> MotionSearchListModel::filterRegions() const
{
    return d->filterRegions();
}

void MotionSearchListModel::setFilterRegions(const QList<QRegion>& value)
{
    d->setFilterRegions(value);
}

bool MotionSearchListModel::isFilterEmpty() const
{
    return std::all_of(filterRegions().cbegin(), filterRegions().cend(),
        [](const QRegion& region) { return region.isEmpty(); });
}

bool MotionSearchListModel::isConstrained() const
{
    return !isFilterEmpty() || base_type::isConstrained();
}

bool MotionSearchListModel::isCameraApplicable(const QnVirtualCameraResourcePtr& camera) const
{
    return base_type::isCameraApplicable(camera) && d->isCameraApplicable(camera);
}

} // namespace nx::vms::client::desktop
