#ifndef BUSINESS_EVENTS_FILTER_KVPAIR_ADAPTER_H
#define BUSINESS_EVENTS_FILTER_KVPAIR_ADAPTER_H

#include <QtCore/QObject>

#include <api/model/kvpair.h>
#include <business/business_fwd.h>
#include <core/resource/resource_fwd.h>

class QnBusinessEventsFilterKvPairAdapter : public QObject
{
    Q_OBJECT
public:
    explicit QnBusinessEventsFilterKvPairAdapter(const QnResourcePtr &resource, QObject *parent = 0);

    bool eventAllowed(BusinessEventType::Value eventType) const;
    static bool eventAllowed(const QnResourcePtr &resource, BusinessEventType::Value eventType);

    static QString key();

    quint64 value() const;
    static quint64 defaultValue();
signals:
    void valueChanged(quint64 value);

private slots:
    void at_valueByKeyChanged(const QnResourcePtr &resource, const QnKvPair &kvPair);
    void at_valueByKeyRemoved(const QnResourcePtr &resource, const QString &key);

private:
    quint64 m_value;
};

#endif // BUSINESS_EVENTS_FILTER_KVPAIR_ADAPTER_H
