#include "engine.h"

#include <nx/vms/api/rules/rule.h>

#include "event_connector.h"
#include "action_executor.h"

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

bool Engine::addActionExecutor(const QString& actionType, ActionExecutor* actionExecutor)
{
    if (m_executors.contains(actionType))
        return false; // TODO: #spanasenko Verbose output?

    m_executors[actionType] = actionExecutor;
    return true;
}

bool Engine::addRule(const api::Rule& serialized)
{
    if (auto rule = buildRule(serialized))
    {
        m_rules[serialized.id] = rule;
        return true;
    }

    return false;
}

bool Engine::registerActionType(const QString& type, const ActionConstructor& ctor)
{
    if (m_actionTypes.contains(type))
        return false; // TODO: #spanasenko Verbose output?

    m_actionTypes[type] = ctor;
    return true;
}

bool Engine::registerEventField(const QString& type, const EventFieldConstructor& ctor)
{
    if (m_eventFields.contains(type))
        return false;

    m_eventFields[type] = ctor;
    return true;
}

bool Engine::registerActionField(const QString& type, const ActionFieldConstructor& ctor)
{
    if (m_actionFields.contains(type))
        return false;

    m_actionFields[type] = ctor;
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
        connect(builder, &ActionBuilder::action, this, &Engine::processAction); // TODO: Move out.
    }

    rule->setComment(serialized.comment);
    rule->setEnabled(serialized.enabled);
    rule->setSchedule(serialized.schedule);

    return rule.take();
}

api::Rule Engine::serialize(const Rule* rule) const
{
    NX_ASSERT(rule);

    api::Rule result;

    for (auto filter: rule->eventFilters())
    {
        result.eventList += serialize(filter);
    }
    for (auto builder: rule->actionBuilders())
    {
        result.actionList += serialize(builder);
    }
    result.comment = rule->comment();
    result.enabled = rule->enabled();
    result.schedule = rule->schedule();

    return result;
}

EventFilter* Engine::buildEventFilter(const api::EventFilter& serialized) const
{
    QScopedPointer<EventFilter> filter(new EventFilter(serialized.id, serialized.eventType));

    for (const auto& fieldInfo: serialized.fields)
    {
        EventField* field = buildEventField(fieldInfo);
        if (!field)
            return nullptr;

        filter->addField(fieldInfo.name, field);
    }

    return filter.take();
}

api::EventFilter Engine::serialize(const EventFilter *filter) const
{
    NX_ASSERT(filter);

    api::EventFilter result;

    const auto fields = filter->fields();
    for (auto it = fields.cbegin(); it != fields.cend(); ++it)
    {
        const auto& name = it.key();
        const auto field = it.value();

        api::Field serialized;
        serialized.name = name;
        serialized.metatype = field->metatype();
        serialized.props = field->serializedProperties();

        result.fields += serialized;
    }
    result.id = filter->id();
    result.eventType = filter->eventType();

    return result;
}

ActionBuilder* Engine::buildActionBuilder(const api::ActionBuilder& serialized) const
{
    ActionConstructor ctor = m_actionTypes.value(serialized.actionType);
    if (!ctor)
        return nullptr;

    QScopedPointer<ActionBuilder> builder(new ActionBuilder(serialized.id, serialized.actionType, ctor));

    for (const auto& fieldInfo: serialized.fields)
    {
        ActionField* field = buildActionField(fieldInfo);
        if (!field)
            return nullptr;

        builder->addField(fieldInfo.name, field);
    }

    builder->setAggregationInterval(serialized.interval);

    return builder.take();
}

api::ActionBuilder Engine::serialize(const ActionBuilder *builder) const
{
    NX_ASSERT(builder);

    api::ActionBuilder result;

    const auto fields = builder->fields();
    for (auto it = fields.cbegin(); it != fields.cend(); ++it)
    {
        const auto& name = it.key();
        const auto field = it.value();

        api::Field serialized;
        serialized.name = name;
        serialized.metatype = field->metatype();
        serialized.props = field->serializedProperties();

        result.fields += serialized;
    }
    result.id = builder->id();
    result.actionType = builder->actionType();

    return result;
}

EventField* Engine::buildEventField(const api::Field& serialized) const
{
    const auto& type = serialized.metatype;
    const auto& ctor = m_eventFields.value(type);
    if (!ctor)
        return nullptr; // TODO: #spanasenko Default field type

    EventField* field = ctor();
    if (!field)
        return nullptr; // Should be unreachable.

    for (auto it = serialized.props.begin(); it != serialized.props.end(); ++it)
    {
        const auto& propName = it.key();
        const auto& propValue = it.value();
        // TODO: #spanasenko Check manifested names.
        field->setProperty(propName.toUtf8().data(), propValue);
    }

    return field;
}

ActionField* Engine::buildActionField(const api::Field& serialized) const
{
    const auto& type = serialized.metatype;
    const auto& ctor = m_actionFields.value(type);
    if (!ctor)
        return nullptr; // TODO: #spanasenko Default field type

    ActionField* field = ctor();
    if (!field)
        return nullptr; // Should be unreachable.

    for (auto it = serialized.props.begin(); it != serialized.props.end(); ++it)
    {
        const auto& propName = it.key();
        const auto& propValue = it.value();
        // TODO: #spanasenko Check manifested names.
        field->setProperty(propName.toUtf8().data(), propValue);
    }

    return field;
}

void Engine::processEvent(const EventPtr& event)
{
    qDebug() << "Processing Event " << event->type();
    qDebug() << event->property("action").toString();

    // TODO: #spanasenko Add filters-by-type maps?
    for (const auto rule: m_rules)
    {
        bool matched = false;
        for (const auto filter: rule->eventFilters())
        {
            if (filter->eventType() == event->type())
            {
                if (filter->match(event))
                {
                    matched = true;
                    break;
                }
            }
        }

        if (matched)
        {
            for (const auto builder: rule->actionBuilders())
            {
                builder->process(event);
            }
        }
    }
}

void Engine::processAction(const ActionPtr& action)
{
    qDebug() << "Processing Action " << action->type();

    if (auto executor = m_executors.value(action->type()))
        executor->execute(action);
}

} // namespace nx::vms::rules
