#include "motion_search_list_model.h"
#include "private/motion_search_list_model_p.h"

namespace nx {
namespace client {
namespace desktop {

MotionSearchListModel::MotionSearchListModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

MotionSearchListModel::~MotionSearchListModel()
{
}


QnVirtualCameraResourcePtr MotionSearchListModel::camera() const
{
    return d->camera();
}

void MotionSearchListModel::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    d->setCamera(camera);
}


bool MotionSearchListModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return d->canFetchMore();
}

void MotionSearchListModel::fetchMore(const QModelIndex& /*parent*/)
{
    d->fetchMore();
}

} // namespace
} // namespace client
} // namespace nx
