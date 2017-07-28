#include "resource_property_adaptors.h"

#include <nx/vms/event/events/abstract_event.h>

using namespace nx;

namespace {
    const QString nameWatchedEventTypes(lit("showBusinessEvents"));


    /* Leaving place for all future events that will be added. They will be watched by default. */
    const quint64 defaultWatchedEventsValue = 0xFFFFFFFFFFFFFFFFull;

    /* Business events are packed to quint64 value, so we need to manually get key for each event.
     * Some events have their value more than 64, so we need to adjust them. */
    qint64 eventTypeKey(vms::event::EventType eventType, bool *overflow) {
        int offset = static_cast<int>(eventType);
        if (offset >= 64) {
            offset = 63;
            if (overflow) {
                NX_ASSERT(!(*overflow), Q_FUNC_INFO, "Code should be improved to support more than one overflowed events.");
                *overflow = true;
            }
        }
        return (1ull << offset);
    }
}

/************************************************************************/
/* QnBusinessEventsFilterResourcePropertyAdaptor                        */
/************************************************************************/

QnBusinessEventsFilterResourcePropertyAdaptor::QnBusinessEventsFilterResourcePropertyAdaptor( QObject *parent /* = NULL*/ )
    : base_type(nameWatchedEventTypes, defaultWatchedEventsValue, parent)
{}

QList<vms::event::EventType> QnBusinessEventsFilterResourcePropertyAdaptor::watchedEvents() const
{
    qint64 packed = value();
    QList<vms::event::EventType> result;
    bool overflow = false;

    for (const vms::event::EventType eventType: vms::event::allEvents()) {
        quint64 key = eventTypeKey(eventType, &overflow);
        if ((packed & key) == key)
            result << eventType;
    }
    return result;
}

void QnBusinessEventsFilterResourcePropertyAdaptor::setWatchedEvents( const QList<vms::event::EventType> &events )
{
    quint64 value = defaultWatchedEventsValue;

    bool overflow = false;
    for (const vms::event::EventType eventType: vms::event::allEvents()) {
        if (events.contains(eventType))
            continue;

        quint64 key = eventTypeKey(eventType, &overflow);
        value &= ~key;
    }
    setValue(value);
}

bool QnBusinessEventsFilterResourcePropertyAdaptor::isAllowed( vms::event::EventType eventType ) const {
    return watchedEvents().contains(eventType);
}

