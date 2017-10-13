#include "event_list_model.h"
#include "private/event_list_model_p.h"

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

        // TODO: #vkutin We might want brief description for Qn::DescriptionTextRole
        // and full description for Qt::ToolTipRole.
        case Qt::ToolTipRole:
        case Qn::DescriptionTextRole:
        case Qt::AccessibleDescriptionRole:
            return QVariant::fromValue(event.description);

        case Qt::ForegroundRole:
            return event.titleColor.isValid()
                ? QVariant::fromValue(event.titleColor)
                : QVariant();

        default:
            return QVariant();
    }
}

bool EventListModel::addEvent(const EventData& event)
{
    return d->addEvent(event);
}

bool EventListModel::removeEvent(const QnUuid& id)
{
    return d->removeEvent(id);
}

void EventListModel::clear()
{
    d->clear();
}

QString EventListModel::timestampText(qint64 timestampMs) const
{
    return QString(); // TODO: #vkutin
}

} // namespace
} // namespace client
} // namespace nx
