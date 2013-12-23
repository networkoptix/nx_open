#include "business_events_filter_kvpair_adapter.h"

#include <core/resource/resource.h>

namespace {
    const QString targetKey = lit("showBusinessEvents");

    const quint64 allIsEnabled = 0xFFFFFFFFFFFFFFFFull;

    quint64 readValue(const QString &encoded) {
        quint64 result = encoded.isEmpty()
                ? allIsEnabled
                : encoded.toULongLong(0, 16);
        return result;
    }

    quint64 readValue(const QnResourcePtr &resource) {
        QString encoded = resource->getProperty(targetKey);
        return readValue(encoded);
    }
}

QnBusinessEventsFilterKvPairAdapter::QnBusinessEventsFilterKvPairAdapter(const QnResourcePtr &resource, QObject *parent) :
    base_type(parent),
    m_value(defaultValue())
{
    if (!resource)
        return;

    m_value = readValue(resource);
    connect(resource, &QnResource::propertyChanged, this, &QnBusinessEventsFilterKvPairAdapter::at_resource_propertyChanged);
}

bool QnBusinessEventsFilterKvPairAdapter::eventAllowed(BusinessEventType::Value eventType) const {
    return (m_value & (1ull << eventType));
}

bool QnBusinessEventsFilterKvPairAdapter::eventAllowed(const QnResourcePtr &resource, BusinessEventType::Value eventType) {
    quint64 value = readValue(resource);
    return (value & (1ull << eventType));
}

QString QnBusinessEventsFilterKvPairAdapter::key() {
    return targetKey;
}

quint64 QnBusinessEventsFilterKvPairAdapter::value() const {
    return m_value;
}

quint64 QnBusinessEventsFilterKvPairAdapter::defaultValue() {
    return allIsEnabled;
}

void QnBusinessEventsFilterKvPairAdapter::at_resource_propertyChanged(const QnResourcePtr &resource, const QString &key) {
    if (key != targetKey)
        return;
    m_value = readValue(resource);
    emit valueChanged(m_value);
}
