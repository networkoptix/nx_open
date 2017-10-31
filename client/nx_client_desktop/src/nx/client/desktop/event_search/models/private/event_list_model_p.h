#pragma once

#include "../event_list_model.h"

#include <QtCore/QList>

namespace nx {
namespace client {
namespace desktop {

class EventListModel::Private: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(EventListModel* q);
    virtual ~Private() override;

    int count() const;

    void clear();

    // Events can be added only in front of the list.
    bool addEvent(const EventData& data);

    // Events can be removed from any place of the list.
    bool removeEvent(const QnUuid& id);

    // Event index lookup by id. Logarithmic complexity.
    int indexOf(const QnUuid& id) const;

    bool updateEvent(const EventData& data);

    const EventData& event(int index) const;

    bool isValid(const QModelIndex& index) const;

    void defaultAction(const QnUuid& id);
    void closeAction(const QnUuid& id);
    void linkAction(const QnUuid& id, const QString& link);

private:
    struct EventDescriptor
    {
        EventData data;
        qint64 sequentialNumber = -1;

        EventDescriptor() = default;
        EventDescriptor(const EventData& data, qint64 sequentialNumber):
            data(data),
            sequentialNumber(sequentialNumber)
        {
        }
    };

private:
    EventListModel* const q = nullptr;
    QList<EventDescriptor> m_events;
    QHash<QnUuid, qint64> m_sequentialNumberById;
    qint64 m_nextSequentialNumber = 0;
};

} // namespace
} // namespace client
} // namespace nx
