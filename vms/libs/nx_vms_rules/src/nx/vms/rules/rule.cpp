#include "rule.h"

#include <QScopedValueRollback>

#include "event_filter.h"
#include "action_builder.h"

namespace nx::vms::rules {

Rule::Rule(const QnUuid& id): m_id(id)
{
}

Rule::~Rule()
{
    qDeleteAll(m_filters);
    qDeleteAll(m_builders);
}

QnUuid Rule::id() const
{
    return m_id;
}

void Rule::addEventFilter(EventFilter* filter)
{
    insertEventFilter(m_filters.size(), filter);
}

void Rule::addActionBuilder(ActionBuilder* builder)
{
    insertActionBuilder(m_builders.size(), builder);
}

void Rule::insertEventFilter(int index, EventFilter* filter)
{
    // TODO: assert
    m_filters.insert(index, filter);
    updateState();
}

void Rule::insertActionBuilder(int index, ActionBuilder* builder)
{
    // TODO: assert
    m_builders.insert(index, builder);
    updateState();
}

EventFilter* Rule::takeEventFilter(int index)
{
    // TODO: assert
    return m_filters.takeAt(index);
    updateState();
}

ActionBuilder* Rule::takeActionBuilder(int index)
{
    // TODO: assert
    return m_builders.takeAt(index);
    updateState();
}

const QList<EventFilter*> Rule::eventFilters() const
{
    return m_filters;
}

const QList<ActionBuilder*> Rule::actionBuilders() const
{
    return m_builders;
}

void Rule::setComment(const QString& comment)
{
    m_comment = comment;
    updateState();
}

QString Rule::comment() const
{
    return m_comment;
}

void Rule::setEnabled(bool isEnabled)
{
    m_enabled = isEnabled;
    updateState();
}

bool Rule::enabled() const
{
    return m_enabled;
}

void Rule::setSchedule(const QByteArray& schedule)
{
    m_schedule = schedule;
    updateState();
}

QByteArray Rule::schedule() const
{
    return m_schedule;
}


void Rule::connectSignals()
{
    for (auto& filter: m_filters)
    {
        filter->connectSignals();
        connect(filter, &EventFilter::stateChanged, this, &Rule::updateState);
    }
    for (auto& builder: m_builders)
    {
        builder->connectSignals();
        connect(builder, &ActionBuilder::stateChanged, this, &Rule::updateState);
    }
}

void Rule::updateState()
{
    //TODO: #spanasenko Update derived values (error messages, etc.)

    if (m_updateInProgress)
        return;

    QScopedValueRollback<bool> guard(m_updateInProgress, true);
    emit stateChanged();
}

} // namespace nx::vms::rules
