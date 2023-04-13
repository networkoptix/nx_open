// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <QtCore/QMetaProperty>

#include <nx/fusion/serialization/json.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qobject.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/common/utils/schedule.h>
#include <nx/vms/rules/ini.h>
#include <nx/vms/utils/translation/scoped_locale.h>
#include <utils/common/synctime.h>

#include "action_builder.h"
#include "action_builder_field.h"
#include "action_executor.h"
#include "basic_action.h"
#include "basic_event.h"
#include "event_connector.h"
#include "event_filter.h"
#include "event_filter_field.h"
#include "manifest.h"
#include "router.h"
#include "rule.h"
#include "utils/field.h"
#include "utils/serialization.h"
#include "utils/type.h"

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
    m_router(std::move(router)),
    m_eventCache(new EventCache())
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
    connect(eventConnector, &EventConnector::analyticsEvents, this, &Engine::processAnalyticsEvents);
    m_connectors.push_back(eventConnector);

    return true;
}

bool Engine::addActionExecutor(const QString& actionType, ActionExecutor* actionExecutor)
{
    if (m_executors.contains(actionType))
    {
        NX_ASSERT(false, "Executor for action type %1 already registered", actionType);
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

void Engine::resetRules(const std::vector<api::Rule>& rulesData)
{
    m_rules.clear();
    for (const auto& data: rulesData)
        addRule(data);

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

    if (descriptor.flags.testFlag(ItemFlag::prolonged))
    {
        bool hasIntervalField{false};
        bool hasDurationField{false};
        for (const auto& fieldDescriptor: descriptor.fields)
        {
            if (fieldDescriptor.fieldName == utils::kIntervalFieldName)
                hasIntervalField = true;

            if (fieldDescriptor.fieldName == utils::kDurationFieldName)
                hasDurationField = true;

            if (hasIntervalField && hasDurationField)
                break;
        }

        if (hasIntervalField && !hasDurationField)
        {
            NX_ERROR(this, "Interval of action is not supported by the prolonged only action");
            return false;
        }
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
    std::unique_ptr<Rule> rule(new Rule(serialized.id, this));

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
    rule->setSystem(serialized.system);
    rule->setSchedule(nx::vms::common::scheduleToByteArray(serialized.schedule));

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
    result.system = rule->isSystem();
    result.schedule = nx::vms::common::scheduleFromByteArray(rule->schedule());

    return result;
}

EventPtr Engine::buildEvent(const EventData& eventData) const
{
    const QString eventType = eventData.value(utils::kType).toString();
    if (!NX_ASSERT(m_eventTypes.contains(eventType)))
        return EventPtr();

    auto event = EventPtr(m_eventTypes[eventType]());
    deserializeProperties(eventData, event.get());

    return event;
}

EventPtr Engine::cloneEvent(const EventPtr& event) const
{
    if (!NX_ASSERT(event))
        return {};

    auto ctor = m_eventTypes.value(event->type());
    if (!NX_ASSERT(ctor))
        return {};

    auto result = EventPtr(ctor());
    for (const auto& propName:
        nx::utils::propertyNames(event.get(), nx::utils::PropertyAccess::fullAccess))
    {
        result->setProperty(propName.constData(), event->property(propName.constData()));
    }

    return result;
}

ActionPtr Engine::cloneAction(const ActionPtr& action) const
{
    if (!NX_ASSERT(action))
        return {};

    auto ctor = m_actionTypes.value(action->type());
    if (!NX_ASSERT(ctor))
        return {};

    auto result = ActionPtr(ctor());
    for (const auto& propName:
        nx::utils::propertyNames(action.get(), nx::utils::PropertyAccess::fullAccess))
    {
        result->setProperty(propName.constData(), action->property(propName.constData()));
    }

    return result;
}

std::unique_ptr<EventFilter> Engine::buildEventFilter(const api::EventFilter& serialized) const
{
    const auto descriptor = eventDescriptor(serialized.type);
    if (!NX_ASSERT(descriptor, "Descriptor for the '%1' type is not registered", serialized.type))
        return {};

    std::unique_ptr<EventFilter> filter(new EventFilter(serialized.id, serialized.type));

    for (const auto& fieldDescriptor: descriptor->fields)
    {
        std::unique_ptr<EventFilterField> field;
        const auto serializedFieldIt = serialized.fields.find(fieldDescriptor.fieldName);
        if (serializedFieldIt != serialized.fields.end())
        {
            NX_ASSERT(serializedFieldIt->second.type == fieldDescriptor.id);
            field = buildEventField(serializedFieldIt->second);
        }
        else //< As the field isn't present in the received serialized filter, make a default one.
        {
            field = buildEventField(fieldDescriptor.id);
        }

        if (!field)
            return nullptr;

        filter->addField(fieldDescriptor.fieldName, std::move(field));
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
        serialized.type = field->metatype();
        serialized.props = field->serializedProperties();

        result.fields.emplace(name, std::move(serialized));
    }
    result.id = filter->id();
    result.type = filter->eventType();

    return result;
}

std::unique_ptr<ActionBuilder> Engine::buildActionBuilder(const api::ActionBuilder& serialized) const
{
    const auto descriptor = actionDescriptor(serialized.type);
    if (!NX_ASSERT(descriptor, "Descriptor for the '%1' type is not registered", serialized.type))
        return {};

    ActionConstructor ctor = m_actionTypes.value(serialized.type);
    //if (!ctor)
    //    return nullptr;

    std::unique_ptr<ActionBuilder> builder(
        new ActionBuilder(serialized.id, serialized.type, ctor));
    connect(builder.get(), &ActionBuilder::action, this, &Engine::processAction);

    for (const auto& fieldDescriptor: descriptor->fields)
    {
        std::unique_ptr<ActionBuilderField> field;
        const auto serializedFieldIt = serialized.fields.find(fieldDescriptor.fieldName);
        if (serializedFieldIt != serialized.fields.end())
        {
            NX_ASSERT(serializedFieldIt->second.type == fieldDescriptor.id);
            field = buildActionField(serializedFieldIt->second);
        }
        else //< As the field isn't present in the received serialized builder, make a default one.
        {
            field = buildActionField(fieldDescriptor.id);
        }

        if (!field)
            return nullptr;

        builder->addField(fieldDescriptor.fieldName, std::move(field));
    }

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
        serialized.type = field->metatype();
        serialized.props = field->serializedProperties();

        result.fields.emplace(name, std::move(serialized));
    }
    result.id = builder->id();
    result.type = builder->actionType();

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

std::unique_ptr<EventFilterField> Engine::buildEventField(const api::Field& serialized) const
{
    auto field = buildEventField(serialized.type);
    if (!field)
        return nullptr;

    deserializeProperties(serialized.props, field.get());

    return field;
}

std::unique_ptr<EventFilterField> Engine::buildEventField(const QString& fieldType) const
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

    std::unique_ptr<EventFilterField> field(constructor());

    NX_ASSERT(field, "Field constructor returns nullptr");

    return field;
}

std::unique_ptr<ActionBuilderField> Engine::buildActionField(const api::Field& serialized) const
{
    auto field = buildActionField(serialized.type);
    deserializeProperties(serialized.props, field.get());

    return field;
}

std::unique_ptr<ActionBuilderField> Engine::buildActionField(const QString& fieldType) const
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

    std::unique_ptr<ActionBuilderField> field(constructor());

    NX_ASSERT(field, "Field constructor returns nullptr");

    return field;
}

void Engine::processEvent(const EventPtr& event)
{
    if (!m_enabled)
        return;

    NX_DEBUG(this, "Processing Event: %1, state: %2", event->type(), event->state());

    EventData eventData;
    QSet<QByteArray> eventFields; // TODO: #spanasenko Cache data.
    QSet<QnUuid> ruleIds, resources;

    // TODO: #spanasenko Add filters-by-type maps?
    for (const auto& [id, rule]: m_rules)
    {
        if (!rule->enabled())
            continue;

        bool matched = false;
        for (const auto filter: rule->eventFiltersByType(event->type()))
        {
            if (filter->match(event))
            {
                matched = true;
                break;
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

    NX_DEBUG(this, "Matched with %1 rules", ruleIds.size());

    if (!ruleIds.empty())
    {
        eventData = serializeProperties(event.get(), eventFields);
        m_router->routeEvent(eventData, ruleIds, resources);
    }
}

void Engine::processAnalyticsEvents(const std::vector<EventPtr>& events)
{
    for (const auto& event: events)
        processEvent(event);
}

// TODO: #spanasenko Use a wrapper with additional checks instead of QHash.
void Engine::processAcceptedEvent(const QnUuid& ruleId, const EventData& eventData)
{
    NX_DEBUG(this, "Processing accepted event, rule id: %1", ruleId);

    const auto it = m_rules.find(ruleId);
    if (it == m_rules.end()) // Assert?
        return;

    const auto& rule = it->second;

    if (!rule->timeInSchedule(qnSyncTime->currentDateTime()))
    {
        NX_VERBOSE(this, "Time is not in rule schedule, skipping the event.");
        return;
    }

    auto event = buildEvent(eventData);
    if (!NX_ASSERT(event))
        return;

    for (const auto builder: rule->actionBuilders())
    {
        if (auto executor = m_executors.value(builder->actionType()))
        {
            nx::vms::utils::ScopedLocalePtr scopedLocale = executor->translateAction(
                builder->actionType());
            builder->process(event);
        }
    }
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

EventCache* Engine::eventCache() const
{
    return m_eventCache.get();
}

} // namespace nx::vms::rules
