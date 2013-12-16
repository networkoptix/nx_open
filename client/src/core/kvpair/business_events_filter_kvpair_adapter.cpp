#include "business_events_filter_kvpair_adapter.h"

#include <core/resource/resource.h>

namespace {
    static const QLatin1String target_key("showBusinessEvents");

    static const quint64 allIsEnabled = 0xFFFFFFFFFFFFFFFFull;

    quint64 readValue(const QString &encoded) {
        quint64 result = encoded.isEmpty()
                ? allIsEnabled
                : encoded.toULongLong(0, 16);
        return result;
    }

    quint64 readValue(const QnResourcePtr &resource) {
        QString encoded = resource->getValueByKey(target_key);
        return readValue(encoded);
    }
}

QnBusinessEventsFilterKvPairAdapter::QnBusinessEventsFilterKvPairAdapter(const QnResourcePtr &resource, QObject *parent) :
    QObject(parent),
    m_value(defaultValue())
{
    if (!resource)
        return;

    m_value = readValue(resource);
    connect(resource.data(), SIGNAL(valueByKeyChanged(QnResourcePtr, QnKvPair)), this, SIGNAL(at_valueByKeyChanged(QnResourcePtr,QnKvPair)));
    connect(resource.data(), SIGNAL(valueByKeyRemoved(QnResourcePtr, QString)), this, SIGNAL(at_valueByKeyRemoved(QnResourcePtr,QString)));
}

bool QnBusinessEventsFilterKvPairAdapter::eventAllowed(BusinessEventType::Value eventType) const {
    return (m_value & (1ull << eventType));
}

bool QnBusinessEventsFilterKvPairAdapter::eventAllowed(const QnResourcePtr &resource, BusinessEventType::Value eventType) {
    quint64 value = readValue(resource);
    return (value & (1ull << eventType));
}

QString QnBusinessEventsFilterKvPairAdapter::key() {
    return target_key;
}

quint64 QnBusinessEventsFilterKvPairAdapter::value() const {
    return m_value;
}

quint64 QnBusinessEventsFilterKvPairAdapter::defaultValue() {
    return allIsEnabled;
}

void QnBusinessEventsFilterKvPairAdapter::at_valueByKeyChanged(const QnResourcePtr &resource, const QnKvPair &kvPair) {
    Q_UNUSED(resource)
    if (kvPair.name() != target_key)
        return;
    m_value = readValue(kvPair.value());
    emit valueChanged(m_value);
}

void QnBusinessEventsFilterKvPairAdapter::at_valueByKeyRemoved(const QnResourcePtr &resource, const QString &key) {
    Q_UNUSED(resource)
    if (key != target_key)
        return;
    m_value = defaultValue();
    emit valueChanged(m_value);
}
