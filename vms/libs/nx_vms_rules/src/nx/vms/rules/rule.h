#pragma once

#include <nx/utils/uuid.h>

namespace nx::vms::rules {

class EventFilter;
class ActionBuilder;

class NX_VMS_RULES_API Rule: public QObject
{
    Q_OBJECT

public:
    Rule(const QnUuid& id);
    ~Rule();

    QnUuid id() const;

    void addEventFilter(EventFilter* filter);
    void addActionBuilder(ActionBuilder* builder);

    void insertEventFilter(int index, EventFilter* filter);
    void insertActionBuilder(int index, ActionBuilder* builder);

    EventFilter* takeEventFilter(int idx);
    ActionBuilder* takeActionBuilder(int idx);

    // TODO: #spanasenko const pointers?
    const QList<EventFilter*> eventFilters() const;
    const QList<ActionBuilder*> actionBuilders() const;

    void setComment(const QString& comment);
    QString comment() const;

    void setEnabled(bool isEnabled);
    bool enabled() const;

    void setSchedule(const QByteArray& schedule);
    QByteArray schedule() const;

    void connectSignals();

signals:
    void stateChanged();

private:
    void updateState();

private:
    QnUuid m_id;

    // TODO: #spanasenko and-or logic
    QList<EventFilter*> m_filters;
    QList<ActionBuilder*> m_builders;

    QString m_comment;
    bool m_enabled;
    QByteArray m_schedule;

    bool m_updateInProgress = false;
};

} // namespace nx::vms::rules