#include "event_search_list_model.h"
#include "private/event_search_list_model_p.h"

#include <chrono>

#include <core/resource/camera_resource.h>
#include <ui/help/help_topics.h>

namespace nx {
namespace client {
namespace desktop {

EventSearchListModel::EventSearchListModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

EventSearchListModel::~EventSearchListModel()
{
}

QnVirtualCameraResourcePtr EventSearchListModel::camera() const
{
    return d->camera();
}

void EventSearchListModel::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    d->setCamera(camera);
}

void EventSearchListModel::clear()
{
    d->clear();
}

vms::event::EventType EventSearchListModel::selectedEventType() const
{
    return d->selectedEventType();
}

void EventSearchListModel::setSelectedEventType(vms::event::EventType value)
{
    d->setSelectedEventType(value);
}

int EventSearchListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d->count();
}

QVariant EventSearchListModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return QVariant();

    const auto& event = d->getEvent(index.row());
    switch (role)
    {
        case Qt::DisplayRole:
            return d->title(event.eventParams.eventType);

        case Qt::DecorationRole:
            return QVariant::fromValue(d->pixmap(event.eventParams));

        case Qt::ForegroundRole:
            return QVariant::fromValue(d->color(event.eventParams.eventType));

        case Qn::DescriptionTextRole:
            return d->description(event.eventParams);

        case Qn::PreviewTimeRole:
            if (!d->hasPreview(event.eventParams.eventType))
                return QVariant();
            /*fallthrough*/
        case Qn::TimestampRole:
            return QVariant::fromValue(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::microseconds(event.eventParams.eventTimestampUsec)).count());

        case Qn::ResourceRole:
            return d->hasPreview(event.eventParams.eventType)
                ? QVariant::fromValue<QnResourcePtr>(d->camera())
                : QVariant();

        case Qn::HelpTopicIdRole:
            return Qn::Empty_Help;

        default:
            return base_type::data(index, role);
    }
}

bool EventSearchListModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return d->canFetchMore();
}

bool EventSearchListModel::prefetchAsync(PrefetchCompletionHandler completionHandler)
{
    return d->prefetch(completionHandler);
}

void EventSearchListModel::commitPrefetch(qint64 keyLimitFromSync)
{
    d->commitPrefetch(keyLimitFromSync);
}

} // namespace
} // namespace client
} // namespace nx
