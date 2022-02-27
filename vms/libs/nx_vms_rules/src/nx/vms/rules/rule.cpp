// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
}

QnUuid Rule::id() const
{
    return m_id;
}

void Rule::addEventFilter(std::unique_ptr<EventFilter> filter)
{
    insertEventFilter(m_filters.size(), std::move(filter));
}

void Rule::addActionBuilder(std::unique_ptr<ActionBuilder> builder)
{
    insertActionBuilder(m_builders.size(), std::move(builder));
}

void Rule::insertEventFilter(int index, std::unique_ptr<EventFilter> filter)
{
    // TODO: assert, connect signals
    m_filters.insert(m_filters.begin() + index, std::move(filter));
    updateState();
}

void Rule::insertActionBuilder(int index, std::unique_ptr<ActionBuilder> builder)
{
    // TODO: assert, connect signals
    m_builders.insert(m_builders.begin() + index, std::move(builder));
    updateState();
}

std::unique_ptr<EventFilter> Rule::takeEventFilter(int index)
{
    // TODO: assert, disconnect
    auto result = std::move(m_filters.at(index));
    m_filters.erase(m_filters.begin() + index);
    updateState();
    return result;
}

std::unique_ptr<ActionBuilder> Rule::takeActionBuilder(int index)
{
    // TODO: assert, disconnect
    auto result = std::move(m_builders.at(index));
    m_builders.erase(m_builders.begin() + index);
    updateState();
    return result;
}

const QList<EventFilter*> Rule::eventFilters() const
{
    QList<EventFilter*> result;
    result.reserve(m_filters.size());
    for (const auto& filter: m_filters)
    {
        result += filter.get();
    }
    return result;
}

const QList<ActionBuilder*> Rule::actionBuilders() const
{
    QList<ActionBuilder*> result;
    result.reserve(m_builders.size());
    for (const auto& builder: m_builders)
    {
        result += builder.get();
    }
    return result;
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
        connect(filter.get(), &EventFilter::stateChanged, this, &Rule::updateState);
    }
    for (auto& builder: m_builders)
    {
        builder->connectSignals();
        connect(builder.get(), &ActionBuilder::stateChanged, this, &Rule::updateState);
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
