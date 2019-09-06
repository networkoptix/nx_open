#pragma once

#include <QObject>

#include "basic_event.h"

namespace nx::vms::rules {

class EventField;

class NX_VMS_RULES_API /*FieldBased*/EventFilter: public QObject
{
    Q_OBJECT

public:
    EventFilter(const QnUuid& id, const QString& eventType);
    virtual ~EventFilter();

    QnUuid id() const;
    QString eventType() const;

    // Takes ownership.
    void addField(const QString& name, EventField* field);

    const QHash<QString, EventField*>& fields() const;

    bool match(const EventPtr& event) const;

    void connectSignals();

signals:
    void stateChanged();

private:
    void updateState();

private:
    QnUuid m_id;
    QString m_eventType;
    QHash<QString, EventField*> m_fields;
    bool m_updateInProgress = false;
};

} // namespace nx::vms::rules