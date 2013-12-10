#include "business_events_filter_kvpair_watcher.h"

namespace {
    static const QLatin1String watcherId("showBusinessEvents");
    static const quint64 defaultValue = 0xFFFFFFFFFFFFFFFFull;
}

QnBusinessEventsFilterKvPairWatcher::QnBusinessEventsFilterKvPairWatcher(QObject *parent) :
    base_type(parent) {
}

QnBusinessEventsFilterKvPairWatcher::~QnBusinessEventsFilterKvPairWatcher() {
}

QString QnBusinessEventsFilterKvPairWatcher::key() const {
    return watcherId;
}

void QnBusinessEventsFilterKvPairWatcher::updateValue(int resourceId, const QString &value) {
    quint64 newValue = value.isEmpty()
            ? defaultValue
            : value.toULongLong(0, 16);

    if (combinedValue(resourceId) == newValue)
        return;

    if(value.isEmpty()) {
        m_valueByResourceId.remove(resourceId);
    } else {
        m_valueByResourceId[resourceId] = newValue;
    }
    emit valueChanged(resourceId, newValue);
}

void QnBusinessEventsFilterKvPairWatcher::removeValue(int resourceId) {
    if (combinedValue(resourceId) == defaultValue)
        return;
    m_valueByResourceId.remove(resourceId);
    emit valueChanged(resourceId, defaultValue);
}

bool QnBusinessEventsFilterKvPairWatcher::eventAllowed(int resourceId, BusinessEventType::Value eventType) const {
    if (!m_valueByResourceId.contains(resourceId))
        return true;
    return (m_valueByResourceId[resourceId] & (1ull << eventType));
}

quint64 QnBusinessEventsFilterKvPairWatcher::combinedValue(int resourceId) const {
    if (!m_valueByResourceId.contains(resourceId))
        return defaultValue;
    return m_valueByResourceId[resourceId];
}

void QnBusinessEventsFilterKvPairWatcher::setCombinedValue(int resourceId, quint64 value) {
    m_valueByResourceId[resourceId] = value;
    submitValue(resourceId, QString::number(value, 16));
}

quint64 QnBusinessEventsFilterKvPairWatcher::defaultCombinedValue() {
    return defaultValue;
}
