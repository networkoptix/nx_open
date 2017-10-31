#include "event_list_model.h"
#include "private/event_list_model_p.h"

#include <QtCore/QDateTime>

#include <utils/common/synctime.h>

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

    const auto& event = d->event(index.row());
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
            return QVariant::fromValue(event.timestamp);

        case Qn::TimestampTextRole:
            return QVariant::fromValue(timestampText(event.timestamp));

        case Qn::ActionIdRole:
            return QVariant::fromValue(event.actionId);

        case Qn::ActionParametersRole:
            return QVariant::fromValue(event.actionParameters);

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

        case Qn::RemovableRole:
            return event.removable;

        default:
            return QVariant();
    }
}

bool EventListModel::addEvent(const EventData& event)
{
    return d->addEvent(event);
}

bool EventListModel::updateEvent(const EventData& event)
{
    return d->updateEvent(event);
}

bool EventListModel::removeEvent(const QnUuid& id)
{
    return d->removeEvent(id);
}

QModelIndex EventListModel::indexOf(const QnUuid& id) const
{
    return index(d->indexOf(id));
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

} // namespace
} // namespace client
} // namespace nx
