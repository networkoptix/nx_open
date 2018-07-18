#include "motion_search_list_model.h"
#include "private/motion_search_list_model_p.h"

#include <chrono>

#include <core/resource/camera_resource.h>
#include <ui/help/business_help.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>

#include <nx/client/core/utils/human_readable.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/event/event_fwd.h>

#include <nx/client/core/watchers/server_time_watcher.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

constexpr qreal kPreviewTimeFraction = 0.5;

std::chrono::microseconds midTime(const QnTimePeriod& period, qreal fraction = kPreviewTimeFraction)
{
    using namespace std::chrono;
    return period.isInfinite()
        ? microseconds(DATETIME_NOW)
        : microseconds(milliseconds(qint64(period.startTimeMs + period.durationMs * fraction)));
}

} // namespace

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

    using namespace std::chrono;
    switch (role)
    {
        case Qt::DisplayRole:
            return tr("Motion on camera");

        case Qt::DecorationRole:
            return qnSkin->pixmap(lit("tree/camera.png"));

        case Qn::HelpTopicIdRole:
            return QnBusiness::eventHelpId(vms::api::EventType::cameraMotionEvent);

        case Qn::TimestampRole:
            return QVariant::fromValue(microseconds(milliseconds(
                d->period(index.row()).startTimeMs)).count());

        case Qn::PreviewTimeRole:
            return QVariant::fromValue(midTime(d->period(index.row())).count());

        case Qn::ForcePrecisePreviewRole:
            return true;

        case Qn::DurationRole:
            return QVariant::fromValue(d->period(index.row()).durationMs);

        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(d->camera());

        case Qn::ContextMenuRole:
            return QVariant::fromValue(d->contextMenu(index.row()));

        case Qn::DescriptionTextRole:
        {
            const auto& period = d->period(index.row());
            const auto timeWatcher = context()->instance<core::ServerTimeWatcher>();
            const auto start = timeWatcher->displayTime(period.startTimeMs);
            return lit("%1: %2<br>%3: %4")
                .arg(tr("Start"))
                .arg(start.toString(Qt::RFC2822Date))
                .arg(tr("Duration"))
                .arg(core::HumanReadable::timeSpan(milliseconds(period.durationMs)));
        }

        default:
            return base_type::data(index, role);
    }
}

// TODO: FIXME!!!
//bool MotionSearchListModel::fetchInProgress() const
//{
//    return d->fetchInProgress();
//}

int MotionSearchListModel::totalCount() const
{
    return d->totalCount();
}

bool MotionSearchListModel::canFetch() const
{
    return d->canFetchMore();
}

void MotionSearchListModel::requestFetch()
{
    d->fetchMore();
    finishFetch(false);
}

void MotionSearchListModel::clearData()
{
    d->reset();
}

void MotionSearchListModel::truncateToRelevantTimePeriod()
{
    // TODO: FIXME!!!
}

void MotionSearchListModel::truncateToMaximumCount()
{
    // TODO: FIXME!!!
}

} // namespace desktop
} // namespace client
} // namespace nx
