// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <nx/utils/log/log.h>
#include <nx/vms/api/rules/rule.h>

#include "event_connector.h"
#include "action_executor.h"
#include "router.h"

#include "event_filter.h"
#include "action_builder.h"
#include "event_field.h"
#include "action_field.h"

template<> nx::vms::rules::Engine* Singleton<nx::vms::rules::Engine>::s_instance = nullptr;

namespace nx::vms::rules {

Engine::Engine(std::unique_ptr<Router> router, QObject* parent):
    QObject(parent),
    m_router(std::move(router))
{
    connect(m_router.get(), &Router::eventReceived, this, &Engine::processAcceptedEvent);
}

void Engine::init(const QnUuid &id, const std::vector<api::Rule>& rules)
{
    m_id = id;
    m_router->init(id);
    for (const auto& rule: rules)
    {
        addRule(rule);
    }
}

bool Engine::addEventConnector(EventConnector *eventConnector)
{
    if (m_connectors.contains(eventConnector))
        return false;

    connect(eventConnector, &EventConnector::ruleEvent, this, &Engine::processEvent);
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
        m_rules[serialized.id] = std::move(rule);
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
    const QJsonObject& /*manifest*/,
    const std::function<bool()>& /*checker*/)
{
    return false;
}

bool Engine::registerActionField(
    const QJsonObject& /*manifest*/,
    const std::function<QJsonValue()>& /*reducer*/)
{
    return false;
}

bool Engine::registerEventType(
    const QJsonObject& /*manifest*/,
    const std::function<EventPtr()>& /*constructor*/ /*, QObject* source*/)
{
    return false;
}

bool Engine::registerActionType(
    const QJsonObject& /*manifest*/,
    const std::function<EventPtr()>& /*constructor*/,
    QObject* /*executor*/)
{
    return false;
}

std::unique_ptr<Rule> Engine::buildRule(const api::Rule& serialized) const
{
    std::unique_ptr<Rule> rule(new Rule(serialized.id));

    for (const auto& filterInfo: serialized.eventList)
    {
        auto filter = buildEventFilter(filterInfo);
        if (!filter)
            return nullptr;

        rule->addEventFilter(std::move(filter));
    }

    for (const auto& builderInfo: serialized.actionList)
    {
        auto builder = buildActionBuilder(builderInfo);
        if (!builder)
            return nullptr;

        connect(builder.get(), &ActionBuilder::action, this, &Engine::processAction); // TODO: Move out.
        rule->addActionBuilder(std::move(builder));
    }

    rule->setComment(serialized.comment);
    rule->setEnabled(serialized.enabled);
    rule->setSchedule(serialized.schedule);

    return rule;
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

std::unique_ptr<EventFilter> Engine::buildEventFilter(const api::EventFilter& serialized) const
{
    std::unique_ptr<EventFilter> filter(new EventFilter(serialized.id, serialized.eventType));

    for (const auto& fieldInfo: serialized.fields)
    {
        auto field = buildEventField(fieldInfo);
        if (!field)
            return nullptr;

        filter->addField(fieldInfo.name, std::move(field));
    }

    return filter;
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

std::unique_ptr<ActionBuilder> Engine::buildActionBuilder(const api::ActionBuilder& serialized) const
{
    ActionConstructor ctor = m_actionTypes.value(serialized.actionType);
    //if (!ctor)
    //    return nullptr;

    std::unique_ptr<ActionBuilder> builder(new ActionBuilder(serialized.id, serialized.actionType, ctor));

    for (const auto& fieldInfo: serialized.fields)
    {
        auto field = buildActionField(fieldInfo);
        if (!field)
            return nullptr;

        builder->addField(fieldInfo.name, std::move(field));
    }

    builder->setAggregationInterval(serialized.intervalS);

    return builder;
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

std::unique_ptr<EventField> Engine::buildEventField(const api::Field& serialized) const
{
    const auto& type = serialized.metatype;
    const auto& ctor = m_eventFields.value(type);
    if (!ctor)
        return nullptr; // TODO: #spanasenko Default field type

    std::unique_ptr<EventField> field(ctor());
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

std::unique_ptr<ActionField> Engine::buildActionField(const api::Field& serialized) const
{
    const auto& type = serialized.metatype;
    const auto& ctor = m_actionFields.value(type);
    if (!ctor)
        return nullptr; // TODO: #spanasenko Default field type

    std::unique_ptr<ActionField> field(ctor());
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

    EventData data;
    QSet<QString> fields; // TODO: #spanasenko Cache data.
    QSet<QnUuid> ids, resources;

    // TODO: #spanasenko Add filters-by-type maps?
    for (const auto& [id, rule]: m_rules)
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
            for (const auto& builder: rule->actionBuilders())
            {
                fields += builder->requiredEventFields();
            }

            for (const auto& builder: rule->actionBuilders())
            {
                resources += builder->affectedResources(data);
            }

            ids += id;
        }
    }

    if (!ids.empty())
    {
        for (const auto& name: fields)
        {
            data[name] = event->property(name.toUtf8().data());
        }

        m_router->routeEvent(data, ids, resources);
    }
}

// TODO: #spanasenko Use a wrapper with additional checks instead of QHash.
void Engine::processAcceptedEvent(const QnUuid& ruleId, const EventData& eventData)
{
    qDebug() << "Processing accepted event";

    const auto it = m_rules.find(ruleId);
    if (it == m_rules.end()) // Assert?
        return;

    const auto& rule = it->second;

    for (const auto builder: rule->actionBuilders())
    {
        builder->process(eventData);
    }
}

void Engine::processAction(const ActionPtr& action)
{
    qDebug() << "Processing Action " << action->type();

    if (auto executor = m_executors.value(action->type()))
        executor->execute(action);
}

} // namespace nx::vms::rules
