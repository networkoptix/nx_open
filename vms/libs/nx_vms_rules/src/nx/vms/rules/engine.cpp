#include "engine.h"

#include <nx/vms/api/rules/rule.h>

#include "event_connector.h"

#include "event_filter.h"
#include "action_builder.h"
#include "event_field.h"
#include "action_field.h"
#include "rule.h"

namespace nx::vms::rules {

Engine::Engine()
{
}

bool Engine::addEventConnector(EventConnector *eventConnector)
{
    if (m_connectors.contains(eventConnector))
        return false;

    connect(eventConnector, &EventConnector::event, this, &Engine::processEvent);
    m_connectors.push_back(eventConnector);
    return true;
}

bool Engine::registerEventField(
    const QJsonObject& manifest,
    const std::function<bool()>& checker)
{
    return false;
}

bool Engine::registerActionField(
    const QJsonObject& manifest,
    const std::function<QJsonValue()>& reducer)
{
    return false;
}

bool Engine::registerEventType(
    const QJsonObject& manifest,
    const std::function<EventPtr()>& constructor/*, QObject* source*/)
{
    return false;
}

bool Engine::registerActionType(
    const QJsonObject& manifest,
    const std::function<EventPtr()>& constructor,
    QObject* executor)
{
    return false;
}

Rule* Engine::buildRule(const api::Rule& serialized) const
{
    QScopedPointer<Rule> rule(new Rule(serialized.id));

    for (const auto& filterInfo: serialized.eventList)
    {
        EventFilter* filter = buildEventFilter(filterInfo);
        if (!filter)
            return nullptr;

        rule->addEventFilter(filter);
    }

    for (const auto& builderInfo: serialized.actionList)
    {
        ActionBuilder* builder = buildActionBuilder(builderInfo);
        if (!builder)
            return nullptr;

        rule->addActionBuilder(builder);
    }

    rule->setComment(serialized.comment);
    rule->setEnabled(serialized.enabled);
    rule->setSchedule(serialized.schedule);

    return rule.take();
}

EventFilter* Engine::buildEventFilter(const api::EventFilter& serialized) const
{
    QScopedPointer<EventFilter> filter(new EventFilter(serialized.id));

    for (const auto& block: serialized.fieldBlocks)
    {
        for (const auto& fieldInfo: block)
        {
            EventField* field = buildEventField(fieldInfo);
            if (!field)
                return nullptr;

            // builder->addField(field); // XXX
        }
    }

    return filter.take();
}

ActionBuilder* Engine::buildActionBuilder(const api::ActionBuilder& serialized) const
{
    QScopedPointer<ActionBuilder> builder(new ActionBuilder(serialized.id));

    for (const auto& block: serialized.fieldBlocks)
    {
        for (const auto& fieldInfo: block)
        {
            ActionField* field = buildActionField(fieldInfo);
            if (!field)
                return nullptr;

            // builder->addField(field); // XXX
        }
    }

    builder->setAggregationInterval(serialized.interval);

    return builder.take();
}

EventField* Engine::buildEventField(const api::Field& serialized) const
{
    const auto& type = serialized.metatype;
    const auto& ctor = m_eventFields.value(type);
    if (!ctor)
        return nullptr; // TODO: #spanasenko Default field type

    EventField* field = ctor(serialized.id, serialized.name);
    if (!field)
        return nullptr; // Should be unreachable.

    for (auto it = serialized.props.begin(); it != serialized.props.end(); ++it)
    {
        const auto& propName = it.key();
        const auto& propValue = it.value();
        // TODO: #spanasenko Check manifested names.
        field->setProperty(propName.toLatin1().data(), propValue);
    }

    return field;
}

ActionField* Engine::buildActionField(const api::Field& serialized) const
{
    const auto& type = serialized.metatype;
    const auto& ctor = m_actionFields.value(type);
    if (!ctor)
        return nullptr; // TODO: #spanasenko Default field type

    ActionField* field = ctor(serialized.id, serialized.name);
    if (!field)
        return nullptr; // Should be unreachable.

    for (auto it = serialized.props.begin(); it != serialized.props.end(); ++it)
    {
        const auto& propName = it.key();
        const auto& propValue = it.value();
        // TODO: #spanasenko Check manifested names.
        field->setProperty(propName.toLatin1().data(), propValue);
    }

    return field;
}

void Engine::processEvent(const EventPtr &event)
{
    qDebug() << "Processing " << event->type();
    qDebug() << event->property("action").toString();
}

} // namespace nx::vms::rules
