// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <QtCore/QMetaProperty>

#include <nx/fusion/serialization/json.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qobject.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/rules/ini.h>

#include "action_builder.h"
#include "action_executor.h"
#include "action_field.h"
#include "basic_action.h"
#include "basic_event.h"
#include "event_connector.h"
#include "event_field.h"
#include "event_filter.h"
#include "manifest.h"
#include "router.h"
#include "rule.h"
#include "utils/serialization.h"
#include "utils/type.h"

template<> nx::vms::rules::Engine* Singleton<nx::vms::rules::Engine>::s_instance = nullptr;

namespace nx::vms::rules {

namespace {

bool isDescriptorValid(const ItemDescriptor& descriptor)
{
    if (descriptor.id.isEmpty() || descriptor.displayName.isEmpty())
        return false;

    std::set<QString> uniqueFields;
    for (const auto& field: descriptor.fields)
    {
        if (!uniqueFields.insert(field.fieldName).second)
            return false;
    }

    return true;
}

} // namespace

Engine::Engine(std::unique_ptr<Router> router, QObject* parent):
    QObject(parent),
    m_router(std::move(router))
{
    const QString rulesVersion(ini().rulesEngine);
    m_enabled = (rulesVersion == "new" || rulesVersion == "both");
    m_oldEngineEnabled = (rulesVersion != "new");

    connect(m_router.get(), &Router::eventReceived, this, &Engine::processAcceptedEvent);
}

Engine::~Engine()
{
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

bool Engine::isEnabled() const
{
    return m_enabled;
}

bool Engine::isOldEngineEnabled() const
{
    return m_oldEngineEnabled;
}

bool Engine::hasRules() const
{
    return !m_rules.empty();
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
    {
        NX_DEBUG(this, "Executor for action type %1 already registered", actionType);
        return false;
    }

    m_executors.insert(actionType, actionExecutor);
    return true;
}

const std::unordered_map<QnUuid, const Rule*> Engine::rules() const
{
    std::unordered_map<QnUuid, const Rule*> result;
    for (const auto& i: m_rules)
        result.insert(std::make_pair(i.first, i.second.get()));

    return result;
}

const Rule* Engine::rule(const QnUuid& id) const
{
    if (m_rules.contains(id))
        return m_rules.at(id).get();

    return nullptr;
}

Engine::RuleSet Engine::cloneRules() const
{
    RuleSet result;

    for (const auto& rule: m_rules)
    {
        if (auto cloned = cloneRule(rule.first))
            result[rule.first] = std::move(cloned);
    }

    return result;
}

std::unique_ptr<Rule> Engine::cloneRule(const QnUuid& id) const
{
    if (!m_rules.contains(id))
        return {};

    auto cloned = buildRule(serialize(m_rules.at(id).get()));

    NX_ASSERT(cloned, "Failed to clone existing rule");

    return cloned;
}

bool Engine::updateRule(const api::Rule& ruleData)
{
    if (auto rule = buildRule(ruleData))
        return updateRule(std::move(rule));

    return false;
}

bool Engine::updateRule(std::unique_ptr<Rule> rule)
{
    auto ruleId = rule->id();
    auto result = m_rules.insert_or_assign(ruleId, std::move(rule));

    emit ruleAddedOrUpdated(ruleId, result.second);
    return result.second;
}

void Engine::removeRule(QnUuid ruleId)
{
    auto it = m_rules.find(ruleId);
    if (it == m_rules.end())
        return;

    m_rules.erase(it);
    emit ruleRemoved(ruleId);
}

void Engine::setRules(RuleSet&& rules)
{
    m_rules = std::move(rules);
    emit rulesReset();
}

bool Engine::registerEvent(const ItemDescriptor& descriptor, const EventConstructor& constructor)
{
    if (!isDescriptorValid(descriptor))
    {
        NX_ERROR(this, "Register event failed: invalid descriptor passed");
        return false;
    }

    if (m_eventDescriptors.contains(descriptor.id))
    {
        NX_ERROR(this, "Register event failed: % is already registered", descriptor.id);
        return false;
    }

    bool hasUnregisteredFields = !std::all_of(
        descriptor.fields.cbegin(),
        descriptor.fields.cend(),
        [this](const FieldDescriptor& fieldDescriptor)
        {
            return isEventFieldRegistered(fieldDescriptor.id);
        });

    if (hasUnregisteredFields)
    {
        NX_ERROR(
            this,
            "Register event failed: %1 descriptor has unregistered fields",
            descriptor.id);
        return false;
    }

    if (!registerEventConstructor(descriptor.id, constructor))
        return false;

    m_eventDescriptors.insert(descriptor.id, descriptor);

    return true;
}

bool Engine::registerEventConstructor(const QString& id, const EventConstructor& constructor)
{
    if (!constructor)
    {
        NX_ERROR(this, "Register %1 event constructor failed, invalid constructor passed", id);
        return false;
    }

    if (m_eventTypes.contains(id))
    {
        NX_ERROR(
            this,
            "Register event constructor failed: %1 constructor is already registered",
            id);
        return false;
    }

    m_eventTypes.insert(id, constructor);
    return true;
}

const QMap<QString, ItemDescriptor>& Engine::events() const
{
    return m_eventDescriptors;
}

std::optional<ItemDescriptor> Engine::eventDescriptor(const QString& eventId) const
{
    return m_eventDescriptors.contains(eventId)
        ? std::optional<ItemDescriptor>(m_eventDescriptors.value(eventId))
        : std::nullopt;
}

bool Engine::registerAction(const ItemDescriptor& descriptor, const ActionConstructor& constructor)
{
    if (!isDescriptorValid(descriptor))
    {
        NX_ERROR(this, "Register action failed: invalid descriptor passed");
        return false;
    }

    if (m_actionDescriptors.contains(descriptor.id))
    {
        NX_ERROR(this, "Register action failed: % is already registered", descriptor.id);
        return false;
    }

    for (const auto& field: descriptor.fields)
    {
        if (isActionFieldRegistered(field.id))
            continue;

        NX_ERROR(
            this,
            "Register action failed: %1, descriptor has unregistered field: %2",
            descriptor.id,
            field.id);

        return false;
    }

    if (!registerActionConstructor(descriptor.id, constructor))
        return false;

    m_actionDescriptors.insert(descriptor.id, descriptor);

    return true;
}

bool Engine::registerActionConstructor(const QString& id, const ActionConstructor& constructor)
{
    if (!constructor)
    {
        NX_ERROR(this, "Register %1 action constructor failed, invalid constructor passed", id);
        return false;
    }

    if (m_actionTypes.contains(id))
    {
        NX_ERROR(
            this,
            "Register action constructor failed: %1 constructor is already registered",
            id);
        return false;
    }

    m_actionTypes.insert(id, constructor);
    return true;
}

const QMap<QString, ItemDescriptor>& Engine::actions() const
{
    return m_actionDescriptors;
}

std::optional<ItemDescriptor> Engine::actionDescriptor(const QString& actionId) const
{
    return m_actionDescriptors.contains(actionId)
        ? std::optional<ItemDescriptor>(m_actionDescriptors.value(actionId))
        : std::nullopt;
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

bool Engine::addRule(const api::Rule& serialized)
{
    if (auto rule = buildRule(serialized))
    {
        m_rules[serialized.id] = std::move(rule);
        return true;
    }

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

    result.id = rule->id();

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

EventPtr Engine::buildEvent(const EventData& eventData) const
{
    const QString eventType = eventData.value(utils::kType).toString();
    if (!NX_ASSERT(m_eventTypes.contains(eventType)))
        return EventPtr();

    auto eventConstructor = m_eventTypes.value(eventType);
    auto event = EventPtr(eventConstructor());
    auto propertyNames = nx::utils::propertyNames(event.get(), nx::utils::PropertyAccess::fullAccess);
    for (const auto& propertyName: propertyNames)
    {
        if (eventData.contains(propertyName))
            event->setProperty(propertyName.toUtf8(), eventData.value(propertyName));
    }

    return event;
}

std::unique_ptr<EventFilter> Engine::buildEventFilter(const api::EventFilter& serialized) const
{
    if (serialized.eventType.isEmpty())
        return nullptr;

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

std::unique_ptr<EventFilter> Engine::buildEventFilter(const QString& eventType) const
{
    if (!m_eventDescriptors.contains(eventType))
    {
        NX_ERROR(
            this,
            "Failed to build event filter as an event descriptor for the %1 event is not registered",
            eventType);

        return {};
    }

    return buildEventFilter(m_eventDescriptors.value(eventType));
}

std::unique_ptr<EventFilter> Engine::buildEventFilter(const ItemDescriptor& descriptor) const
{
    const auto id = QUuid::createUuid();

    std::unique_ptr<EventFilter> filter(new EventFilter(id, descriptor.id));

    for (const auto& fieldDescriptor: descriptor.fields)
    {
        auto field = buildEventField(fieldDescriptor.id);
        if (!field)
        {
            NX_ERROR(
                this,
                "Failed to build event filter as an event field for the %1 type was not built",
                fieldDescriptor.id);

            return nullptr;
        }

        field->setProperties(fieldDescriptor.properties);

        filter->addField(fieldDescriptor.fieldName, std::move(field));
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

    std::unique_ptr<ActionBuilder> builder(
        new ActionBuilder(serialized.id, serialized.actionType, ctor));
    connect(builder.get(), &ActionBuilder::action, this, &Engine::processAction);

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

std::unique_ptr<ActionBuilder> Engine::buildActionBuilder(const QString& actionType) const
{
    if (!m_actionDescriptors.contains(actionType))
    {
        NX_ERROR(
            this,
            "Failed to build action builder as action descriptor for the %1 action is not registered",
            actionType);

        return {};
    }

    return buildActionBuilder(m_actionDescriptors.value(actionType));
}

std::unique_ptr<ActionBuilder> Engine::buildActionBuilder(const ItemDescriptor& descriptor) const
{
    ActionConstructor constructor = m_actionTypes.value(descriptor.id);

    auto builder =
        std::make_unique<ActionBuilder>(QnUuid::createUuid(), descriptor.id, constructor);

    connect(builder.get(), &ActionBuilder::action, this, &Engine::processAction);

    for (const auto& fieldDescriptor: descriptor.fields)
    {
        auto field = buildActionField(fieldDescriptor.id);
        if (!field)
        {
            NX_ERROR(
                this,
                "Failed to build action builder as an action field for the %1 type was not built",
                fieldDescriptor.id);

            return nullptr;
        }

        field->setProperties(fieldDescriptor.properties);

        builder->addField(fieldDescriptor.fieldName, std::move(field));
    }

    return builder;
}

std::unique_ptr<EventField> Engine::buildEventField(const api::Field& serialized) const
{
    auto field = buildEventField(serialized.metatype);
    if (!field)
        return nullptr;

    deserializeProperties(serialized.props, field.get());

    return field;
}

std::unique_ptr<EventField> Engine::buildEventField(const QString& fieldType) const
{
    const auto& constructor = m_eventFields.value(fieldType);
    if (!constructor)
    {
        NX_ERROR(
            this,
            "Failed to build event field as an event field for the %1 type is not registered",
            fieldType);

        return nullptr; // TODO: #spanasenko Default field type
    }

    std::unique_ptr<EventField> field(constructor());

    NX_ASSERT(field, "Field constructor returns nullptr");

    return field;
}

std::unique_ptr<ActionField> Engine::buildActionField(const api::Field& serialized) const
{
    auto field = buildActionField(serialized.metatype);

    for (auto it = serialized.props.begin(); it != serialized.props.end(); ++it)
    {
        const auto& propName = it.key();
        const auto& propValue = it.value();
        // TODO: #spanasenko Check manifested names.
        field->setProperty(propName.toUtf8().data(), propValue);
    }

    return field;
}

std::unique_ptr<ActionField> Engine::buildActionField(const QString& fieldType) const
{
    const auto& constructor = m_actionFields.value(fieldType);
    if (!constructor)
    {
        NX_ERROR(
            this,
            "Failed to build action field as an action field for the %1 type is not registered",
            fieldType);

        return nullptr; // TODO: #spanasenko Default field type
    }

    std::unique_ptr<ActionField> field(constructor());

    NX_ASSERT(field, "Field constructor returns nullptr");

    return field;
}

void Engine::processEvent(const EventPtr& event)
{
    if (!m_enabled)
        return;

    NX_DEBUG(this, "Processing Event: %1, state: %2", event->type(), event->state());

    EventData eventData;
    QSet<QString> eventFields; // TODO: #spanasenko Cache data.
    QSet<QnUuid> ruleIds, resources;

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
            //for (const auto& builder: rule->actionBuilders())
            //{
            //    eventFields += builder->requiredEventFields();
            //}

            //for (const auto& builder: rule->actionBuilders())
            //{
            //    resources += builder->affectedResources(eventData);
            //}

            for (const auto& fieldName: nx::utils::propertyNames(event.get()))
            {
                eventFields += fieldName;
            }

            ruleIds += id;
        }
    }

    if (!ruleIds.empty())
    {
        for (const auto& name: eventFields)
        {
            // TODO: #spanasenko Replace by custom convertors (!)
            eventData[name] = event->property(name.toUtf8().data());
        }

        m_router->routeEvent(eventData, ruleIds, resources);
    }
}

// TODO: #spanasenko Use a wrapper with additional checks instead of QHash.
void Engine::processAcceptedEvent(const QnUuid& ruleId, const EventData& eventData)
{
    NX_DEBUG(this, "Processing accepted event, rule id: %1", ruleId);

    const auto it = m_rules.find(ruleId);
    if (it == m_rules.end()) // Assert?
        return;

    const auto& rule = it->second;

    auto event = buildEvent(eventData);
    if (!NX_ASSERT(event))
        return;

    for (const auto builder: rule->actionBuilders())
        builder->process(event);
}

void Engine::processAction(const ActionPtr& action)
{
    NX_DEBUG(this, "Processing Action %1", action->type());

    if (auto executor = m_executors.value(action->type()))
        executor->execute(action);
}

bool Engine::isEventFieldRegistered(const QString& fieldId) const
{
    return m_eventFields.contains(fieldId);
}

bool Engine::isActionFieldRegistered(const QString& fieldId) const
{
    return m_actionFields.contains(fieldId);
}

} // namespace nx::vms::rules
