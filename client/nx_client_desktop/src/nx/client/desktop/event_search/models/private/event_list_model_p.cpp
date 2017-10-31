#include "event_list_model_p.h"

#include <QtGui/QDesktopServices>

#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {

EventListModel::Private::Private(EventListModel* q):
    base_type(),
    q(q)
{
}

EventListModel::Private::~Private()
{
}

int EventListModel::Private::count() const
{
    return m_events.count();
}

bool EventListModel::Private::addEvent(const EventData& data)
{
    if (m_sequentialNumberById.contains(data.id))
        return false;

    ScopedInsertRows insertRows(q, QModelIndex(), 0, 0);
    m_events.push_front({data, m_nextSequentialNumber});
    m_sequentialNumberById[data.id] = m_nextSequentialNumber;
    ++m_nextSequentialNumber;
    return true;
}

bool EventListModel::Private::removeEvent(const QnUuid& id)
{
    const auto index = indexOf(id);
    if (index < 0)
        return false;

    q->beforeRemove(m_events[index].data);

    ScopedRemoveRows removeRows(q, QModelIndex(), index, index);
    m_sequentialNumberById.remove(m_events[index].data.id);
    m_events.removeAt(index);
    return true;
}

void EventListModel::Private::clear()
{
    ScopedReset reset(q);
    m_events.clear();
    m_nextSequentialNumber = 0;
}

bool EventListModel::Private::updateEvent(const EventData& data)
{
    const auto i = indexOf(data.id);
    if (i < 0)
        return false;

    m_events[i].data = data;

    const auto index = q->index(i);
    emit q->dataChanged(index, index);

    return true;
}

const EventListModel::EventData& EventListModel::Private::getEvent(int index) const
{
    if (index < 0 || index >= m_events.count())
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Index is out of range");
        static const EventData dummy;
        return dummy;
    }

    return m_events[index].data;
}

int EventListModel::Private::indexOf(const QnUuid& id) const
{
    const auto iter = m_sequentialNumberById.find(id);
    if (iter == m_sequentialNumberById.end())
        return -1;

    const auto sequentialNumber = iter.value();
    const auto found = std::lower_bound(m_events.cbegin(), m_events.cend(), sequentialNumber,
        [](const EventDescriptor& left, qint64 right)
        {
            return left.sequentialNumber > right;
        });

    return found != m_events.cend()
        ? std::distance(m_events.cbegin(), found)
        : -1;
}

bool EventListModel::Private::isValid(const QModelIndex& index) const
{
    return index.model() == q && !index.parent().isValid() && index.column() == 0
        && index.row() >= 0 && index.row() < count();
}

void EventListModel::Private::defaultAction(const QnUuid& id)
{
    const auto index = indexOf(id);
    NX_ASSERT(index >= 0, Q_FUNC_INFO, "Unknown event id");
    if (index < 0)
        return;

    q->triggerDefaultAction(m_events[index].data);
}

void EventListModel::Private::closeAction(const QnUuid& id)
{
    const auto index = indexOf(id);
    NX_ASSERT(index >= 0, Q_FUNC_INFO, "Unknown event id");
    if (index < 0)
        return;

    q->triggerCloseAction(m_events[index].data);
}

void EventListModel::Private::linkAction(const QnUuid& id, const QString& link)
{
    const auto index = indexOf(id);
    NX_ASSERT(index >= 0, Q_FUNC_INFO, "Unknown event id");
    if (index < 0)
        return;

    q->triggerLinkAction(m_events[index].data, link);
}

} // namespace
} // namespace client
} // namespace nx
