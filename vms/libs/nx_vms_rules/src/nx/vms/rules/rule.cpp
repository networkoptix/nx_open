// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rule.h"

#include <QtCore/QDateTime>
#include <QtCore/QScopedValueRollback>

#include "action_builder.h"
#include "event_filter.h"

namespace nx::vms::rules {

Rule::Rule(const QnUuid& id, const Engine* engine): m_id(id), m_engine(engine)
{
}

Rule::~Rule()
{
}

QnUuid Rule::id() const
{
    return m_id;
}

const Engine* Rule::engine() const
{
    return m_engine;
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
    NX_ASSERT(filter);
    filter->setRule(this);
    m_filters.insert(m_filters.begin() + index, std::move(filter));
    updateState();
}

void Rule::insertActionBuilder(int index, std::unique_ptr<ActionBuilder> builder)
{
    // TODO: assert, connect signals
    NX_ASSERT(builder);
    builder->setRule(this);
    m_builders.insert(m_builders.begin() + index, std::move(builder));
    updateState();
}

std::unique_ptr<EventFilter> Rule::takeEventFilter(int index)
{
    // TODO: assert, disconnect
    auto result = std::move(m_filters.at(index));
    result->setRule({});
    m_filters.erase(m_filters.begin() + index);
    updateState();
    return result;
}

std::unique_ptr<ActionBuilder> Rule::takeActionBuilder(int index)
{
    // TODO: assert, disconnect
    auto result = std::move(m_builders.at(index));
    result->setRule({});
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

const QList<EventFilter*> Rule::eventFiltersByType(const QString& type) const
{
    QList<EventFilter*> result;

    for (const auto& filter: m_filters)
    {
        if (filter->eventType() == type)
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

bool Rule::timeInSchedule(QDateTime datetime) const
{
    if (m_schedule.isEmpty())
        return true;

    int currentWeekHour = (datetime.date().dayOfWeek()-1)*24 + datetime.time().hour();

    int byteOffset = currentWeekHour / 8;
    if (byteOffset >= m_schedule.size())
        return false;

    int bitNum = 7 - (currentWeekHour % 8);
    quint8 mask = 1 << bitNum;

    return (m_schedule.at(byteOffset) & mask);
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
