#include "motion_search_list_model.h"
#include "private/motion_search_list_model_p.h"

#include <chrono>

#include <core/resource/camera_resource.h>
#include <ui/help/business_help.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include <nx/client/core/utils/human_readable.h>
#include <nx/vms/event/event_fwd.h>

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

int MotionSearchListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d->count();
}

QVariant MotionSearchListModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return QVariant();

    switch (role)
    {
        case Qt::DisplayRole:
            return tr("Motion on camera");

        case Qt::DecorationRole:
            return qnSkin->pixmap(lit("tree/camera.png"));

        case Qn::HelpTopicIdRole:
            return QnBusiness::eventHelpId(vms::event::cameraMotionEvent);

        case Qn::TimestampRole:
        case Qn::PreviewTimeRole:
            return QVariant::fromValue(d->period(index.row()).startTimeMs);

        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(d->camera());

        case Qn::DescriptionTextRole:
        {
            const auto& period = d->period(index.row());
            const auto timeWatcher = context()->instance<QnWorkbenchServerTimeWatcher>();
            const auto start = timeWatcher->displayTime(period.startTimeMs);
            return lit("%1: %2<br>%3: %4")
                .arg(tr("Start"))
                .arg(start.toString(Qt::RFC2822Date))
                .arg(tr("Duration"))
                .arg(core::HumanReadable::timeSpan(std::chrono::milliseconds(period.durationMs)));
        }

        default:
            return base_type::data(index, role);
    }
}

bool MotionSearchListModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return d->canFetchMore();
}

void MotionSearchListModel::fetchMore(const QModelIndex& /*parent*/)
{
    d->fetchMore();
}

} // namespace desktop
} // namespace client
} // namespace nx
