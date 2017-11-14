#include "event_list_model.h"
#include "private/event_list_model_p.h"

#include <QtCore/QDateTime>

#include <core/resource/camera_resource.h>
#include <utils/common/synctime.h>
#include <utils/common/delayed.h>

#include <nx/client/desktop/ui/actions/action_manager.h>

namespace nx {
namespace client {
namespace desktop {

EventListModel::EventListModel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
}

EventListModel::~EventListModel()
{
}

int EventListModel::rowCount(const QModelIndex& parent) const
{
    return d->count();
}

QVariant EventListModel::data(const QModelIndex& index, int role) const
{
    if (!d->isValid(index))
        return QVariant();

    const auto& event = d->getEvent(index.row());
    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::AccessibleTextRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
            return QVariant::fromValue(event.title);

        case Qt::DecorationRole:
            return QVariant::fromValue(event.icon);

        case Qn::UuidRole:
            return QVariant::fromValue(event.id);

        case Qn::TimestampRole:
            return QVariant::fromValue(event.timestampMs);

        case Qn::PreviewTimeRole:
            return QVariant::fromValue(event.previewTimeMs);

        case Qn::TimestampTextRole:
            return QVariant::fromValue(timestampText(event.timestampMs));

        case Qt::ToolTipRole:
            return QVariant::fromValue(event.toolTip);

        case Qn::DescriptionTextRole:
        case Qt::AccessibleDescriptionRole:
            return QVariant::fromValue(event.description);

        case Qt::ForegroundRole:
            return event.titleColor.isValid()
                ? QVariant::fromValue(event.titleColor)
                : QVariant();

        case Qn::ResourceSearchStringRole:
            return lit("%1 %2").arg(event.title).arg(event.description);

        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(d->previewCamera(event));

        case Qn::ResourceListRole:
            return QVariant::fromValue<QnResourceList>(d->accessibleCameras(event));

        case Qn::RemovableRole:
            return event.removable;

        case Qn::CommandActionRole:
            return QVariant::fromValue(event.extraAction);

        case Qn::PriorityRole:
            return eventPriority(event);

        default:
            return QVariant();
    }
}

bool EventListModel::addEvent(const EventData& event, Position where)
{
    return where == Position::front
        ? d->addFront(event)
        : d->addBack(event);
}

bool EventListModel::updateEvent(const EventData& event)
{
    return d->updateEvent(event);
}

QModelIndex EventListModel::indexOf(const QnUuid& id) const
{
    return index(d->indexOf(id));
}

EventListModel::EventData EventListModel::getEvent(int row) const
{
    return d->getEvent(row);
}

void EventListModel::clear()
{
    d->clear();
}

QString EventListModel::timestampText(qint64 timestampMs) const
{
    if (timestampMs <= 0)
        return QString();

    const auto dateTime = QDateTime::fromMSecsSinceEpoch(timestampMs);
    if (qnSyncTime->currentDateTime().date() != dateTime.date())
        return dateTime.date().toString(Qt::DefaultLocaleShortDate);

    return dateTime.time().toString(Qt::DefaultLocaleShortDate);
}

bool EventListModel::removeEvent(const QnUuid& id)
{
    return d->removeEvent(id);
}

bool EventListModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (parent.isValid() || row < 0 || count < 0 || row + count > rowCount())
        return false;

    d->removeEvents(row, count);
    return true;
}

bool EventListModel::setData(const QModelIndex& index, const QVariant& /*value*/, int role)
{
    if (!index.isValid() || index.model() != this)
        return false;

    switch (role)
    {
        case Qn::DefaultTriggerRole:
        case Qn::ActivateLinkRole:
        {
            const auto& event = getEvent(index.row());
            if (event.actionId != ui::action::NoAction)
                menu()->triggerIfPossible(event.actionId, event.actionParameters);
            return true;
        }

        default:
            return false;
    }
}

int EventListModel::eventPriority(const EventData& /*event*/) const
{
    return 0;
}

bool EventListModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return false;
}

void EventListModel::fetchMore(const QModelIndex& /*parent*/)
{
}

void EventListModel::finishFetch()
{
    executeDelayedParented([this]() { emit fetchFinished(QPrivateSignal()); }, 0, this);
}

} // namespace
} // namespace client
} // namespace nx
