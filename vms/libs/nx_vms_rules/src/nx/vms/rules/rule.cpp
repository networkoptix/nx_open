// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rule.h"

#include <QtCore/QDateTime>
#include <QtCore/QScopedValueRollback>

#include <core/misc/schedule_task.h>

#include "action_builder.h"
#include "action_builder_field.h"
#include "engine.h"
#include "event_filter.h"
#include "event_filter_field.h"
#include "utils/common.h"

namespace nx::vms::rules {

namespace {

template <typename V>
QList<typename V::value_type::pointer> extractPointers(V& v)
{
    QList<typename V::value_type::pointer> result;
    result.reserve(v.size());

    for (const auto& item: v)
        result.append(item.get());

    return result;
}

template <typename V>
QList<const typename V::value_type::element_type*> extractPointers(const V& v)
{
    QList<const typename V::value_type::element_type*> result;
    result.reserve(v.size());

    for (const auto& item: v)
        result.append(item.get());

    return result;
}

} // namespace

Rule::Rule(nx::Uuid id, const Engine* engine): m_id(id), m_engine(engine)
{
}

Rule::~Rule()
{
}

nx::Uuid Rule::id() const
{
    return m_id;
}

void Rule::setId(nx::Uuid id)
{
    m_id = id;
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
    NX_ASSERT(filter);
    NX_ASSERT(index >= 0 && index <= m_filters.size());

    filter->setRule(this);
    m_filters.insert(m_filters.begin() + index, std::move(filter));
}

void Rule::insertActionBuilder(int index, std::unique_ptr<ActionBuilder> builder)
{
    NX_ASSERT(builder);
    NX_ASSERT(index >= 0 && index <= m_builders.size());

    builder->setRule(this);
    m_builders.insert(m_builders.begin() + index, std::move(builder));
}

std::unique_ptr<EventFilter> Rule::takeEventFilter(int index)
{
    NX_ASSERT(index >= 0 && index < m_filters.size());

    auto result = std::move(m_filters.at(index));
    result->setRule({});
    m_filters.erase(m_filters.begin() + index);
    return result;
}

std::unique_ptr<ActionBuilder> Rule::takeActionBuilder(int index)
{
    NX_ASSERT(index >= 0 && index < m_builders.size());

    auto result = std::move(m_builders.at(index));
    result->setRule({});
    m_builders.erase(m_builders.begin() + index);
    return result;
}

QList<EventFilter*> Rule::eventFilters()
{
     return extractPointers(m_filters);
}

QList<const EventFilter*> Rule::eventFilters() const
{
     return extractPointers(m_filters);
}

QList<const EventFilter*> Rule::eventFiltersByType(const QString& type) const
{
    QList<const EventFilter*> result;

    for (const auto& filter: m_filters)
    {
        if (filter->eventType() == type)
            result += filter.get();

    }
    return result;
}

QList<ActionBuilder*> Rule::actionBuilders() const
{
    QList<ActionBuilder*> result;
    result.reserve(m_builders.size());
    for (const auto& builder: m_builders)
    {
        result += builder.get();
    }
    return result;
}

QList<const ActionBuilder*> Rule::actionBuildersByType(const QString& type) const
{
    QList<const ActionBuilder*> result;

    for (const auto& builder: m_builders)
    {
        if (builder->actionType() == type)
            result += builder.get();

    }
    return result;
}

void Rule::setComment(const QString& comment)
{
    m_comment = comment;
}

QString Rule::comment() const
{
    return m_comment;
}

void Rule::setEnabled(bool isEnabled)
{
    m_enabled = isEnabled;
}

bool Rule::enabled() const
{
    return m_enabled;
}

bool Rule::isInternal() const
{
    return m_internal;
}

void Rule::setInternal(bool internal)
{
    m_internal = internal;
}

void Rule::setSchedule(const QByteArray& schedule)
{
    m_schedule = schedule;
}

QByteArray Rule::schedule() const
{
    return m_schedule;
}

bool Rule::timeInSchedule(QDateTime datetime) const
{
    return nx::vms::common::timeInSchedule(datetime, m_schedule);
}

bool Rule::isValid() const
{
    if (m_filters.empty() || m_builders.empty())
        return false;

    if (!utils::isCompatible(
        m_engine, m_filters.front().get(), m_builders.front().get()))
    {
        return false;
    }

    return true;
}

ValidationResult Rule::validity(
    common::SystemContext* context,
    const EventFieldFilter& eventFieldFilter,
    const ActionFieldFilter& actionFieldFilter) const
{
    ValidationResult result;

    QStringList eventFilterAlerts;
    for (const auto& eventFilter: m_filters)
    {
        for (auto [fieldName, field]: eventFilter->fields().asKeyValueRange())
        {
            auto fieldDescriptor = field->descriptor();
            if (!NX_ASSERT(fieldDescriptor))
                return {QValidator::State::Invalid, {}};

            if (eventFieldFilter && !eventFieldFilter(field))
                continue;

            auto validator = m_engine->fieldValidator(field->metatype());
            if (!validator)
                continue;

            const auto fieldValidity = validator->validity(field, this, context);
            result.validity = std::min(result.validity, fieldValidity.validity);

            if (!fieldValidity.description.isEmpty())
            {
                eventFilterAlerts << QString{" - %1(%2): %3"}
                    .arg(fieldName)
                    .arg(field->metatype())
                    .arg(fieldValidity.description);
            }
        }

        if (!eventFilterAlerts.isEmpty())
        {
            eventFilterAlerts.push_front(
                tr("`%1` event filter field alerts:").arg(eventFilter->eventType()));
        }
    }

    QStringList actionBuilderAlerts;
    for (const auto& actionBuilder: m_builders)
    {
        for (auto [fieldName, field]: actionBuilder->fields().asKeyValueRange())
        {
            auto fieldDescriptor = field->descriptor();
            if (!NX_ASSERT(fieldDescriptor))
                return {QValidator::State::Invalid, {}};

            if (actionFieldFilter && !actionFieldFilter(field))
                continue;

            auto validator = m_engine->fieldValidator(field->metatype());
            if (!validator)
                continue;

            const auto fieldValidity = validator->validity(field, this, context);
            result.validity = std::min(result.validity, fieldValidity.validity);

            if (!fieldValidity.description.isEmpty())
            {
                actionBuilderAlerts << QString{" - %1(%2): %3"}
                    .arg(fieldName)
                    .arg(field->metatype())
                    .arg(fieldValidity.description);
            }
        }

        if (!actionBuilderAlerts.isEmpty())
        {
            actionBuilderAlerts.push_front(
                tr("`%1` action builder field alerts:").arg(actionBuilder->actionType()));
        }
    }

    if (const auto alerts = eventFilterAlerts + actionBuilderAlerts; !alerts.isEmpty())
        result.description = alerts.join('\n');

    return result;
}

} // namespace nx::vms::rules
