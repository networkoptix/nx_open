// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <chrono>

#include <QtCore/QMetaProperty>
#include <QtCore/QThread>

#include <nx/fusion/serialization/json.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qobject.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/common/utils/schedule.h>
#include <nx/vms/rules/ini.h>
#include <nx/vms/utils/translation/scoped_locale.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include "action_builder.h"
#include "action_builder_field.h"
#include "action_executor.h"
#include "basic_action.h"
#include "basic_event.h"
#include "event_connector.h"
#include "event_filter.h"
#include "event_filter_field.h"
#include "group.h"
#include "manifest.h"
#include "router.h"
#include "rule.h"
#include "utils/api.h"
#include "utils/field.h"
#include "utils/serialization.h"
#include "utils/type.h"

namespace nx::vms::rules {

using namespace std::chrono;

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
    m_ruleMutex(nx::Mutex::Recursive),
    m_eventCache(std::make_unique<EventCache>()),
    m_aggregationTimer(new QTimer(this))
{
    const QString rulesVersion(ini().rulesEngine);
    m_enabled = (rulesVersion == "new" || rulesVersion == "both");
    m_oldEngineEnabled = (rulesVersion != "new");

    m_aggregationTimer->setInterval(1s);
    connect(m_aggregationTimer, &QTimer::timeout, this,
        [this]()
        {
            NX_MUTEX_LOCKER lock(&m_ruleMutex);

            for (auto handler: m_timerHandlers)
                handler->onTimer(0);
        });

    connect(m_router.get(), &Router::eventReceived, this, &Engine::processAcceptedEvent);
}

Engine::~Engine()
{
    NX_MUTEX_LOCKER lock(&m_ruleMutex);
    m_rules.clear();
}

void Engine::setId(QnUuid id)
{
    if (m_id == id)
        return;

    m_id = id;
    m_router->init(id);
}

bool Engine::isEnabled() const
{
    return m_enabled;
}

bool Engine::isOldEngineEnabled() const
{
    return m_oldEngineEnabled;
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

Engine::ConstRuleSet Engine::rules() const
{
    ConstRuleSet result;
    NX_MUTEX_LOCKER lock(&m_ruleMutex);

    result.reserve(m_rules.size());
    for (const auto& [id, rule]: m_rules)
        result.insert_or_assign(id, rule);

    return result;
}

Engine::ConstRulePtr Engine::rule(const QnUuid& id) const
{
    NX_MUTEX_LOCKER lock(&m_ruleMutex);
    if (const auto it = m_rules.find(id); it != m_rules.end())
        return it->second;

    return {};
}

Engine::RulePtr Engine::cloneRule(const QnUuid& id) const
{
    auto rule = this->rule(id);
    if (!rule)
        return {};

    auto cloned = cloneRule(rule.get());

    NX_ASSERT(cloned, "Failed to clone existing rule, id: %1", id);

    return cloned;
}

std::unique_ptr<Rule> Engine::cloneRule(const Rule* rule) const
{
    return buildRule(serialize(rule));
}

void Engine::updateRule(const api::Rule& ruleData)
{
    auto rule = buildRule(ruleData);
    if (!rule)
        return;

    auto ruleId = rule->id();
    NX_MUTEX_LOCKER lock(&m_ruleMutex);
    auto result = m_rules.insert_or_assign(ruleId, std::move(rule));
    lock.unlock();

    emit ruleAddedOrUpdated(ruleId, result.second);
}

void Engine::removeRule(QnUuid ruleId)
{
    NX_MUTEX_LOCKER lock(&m_ruleMutex);
    const auto erasedCount = m_rules.erase(ruleId);
    lock.unlock();

    if (erasedCount > 0)
        emit ruleRemoved(ruleId);
}

void Engine::resetRules(const std::vector<api::Rule>& rulesData)
{
    RuleSet ruleSet;
    for (const auto& ruleData: rulesData)
    {
        if (auto rule = buildRule(ruleData))
            ruleSet.insert_or_assign(rule->id(), std::move(rule));
    }

    NX_MUTEX_LOCKER lock(&m_ruleMutex);
    m_rules = std::move(ruleSet);
    lock.unlock();

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

Group Engine::eventGroups() const
{
    QMap<std::string, QStringList> eventByGroup;

    for (const auto& descriptor: m_eventDescriptors)
        eventByGroup[descriptor.groupId].push_back(descriptor.id);

    auto result = defaultEventGroups();

    auto visitor =
        [&eventByGroup](Group& group, auto visitor)->void
        {
            group.items = std::move(eventByGroup.value(group.id));
            for (auto& subgroup: group.groups)
                visitor(subgroup, visitor);
        };

    visitor(result, visitor);
    return result;
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

std::unique_ptr<Rule> Engine::buildRule(const api::Rule& serialized) const
{
    auto rule = std::make_unique<Rule>(serialized.id, this);
    rule->moveToThread(thread());

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

EventPtr Engine::buildEvent(const EventData& eventData) const
{
    const QString eventType = eventData.value(utils::kType).toString();
    if (!NX_ASSERT(m_eventTypes.contains(eventType), "Unknown event type: %1", eventType))
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

ActionPtr Engine::buildAction(const EventData& data) const
{
    const QString type = data.value(utils::kType).toString();
    const auto ctor = m_actionTypes.value(type);
    if (!NX_ASSERT(ctor, "Unregistered action constructor: %1", type))
        return {};

    auto action = ActionPtr(ctor());
    deserializeProperties(data, action.get());

    return action;
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

    auto filter = buildEventFilter(serialized.id, serialized.type);

    for (const auto& fieldDescriptor: descriptor->fields)
    {
        std::unique_ptr<EventFilterField> field;
        const auto serializedFieldIt = serialized.fields.find(fieldDescriptor.fieldName);
        if (serializedFieldIt != serialized.fields.end()
            && serializedFieldIt->second.type == fieldDescriptor.id)
        {
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
    auto filter = buildEventFilter(QUuid::createUuid(), descriptor.id);

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

std::unique_ptr<EventFilter> Engine::buildEventFilter(QnUuid id, const QString& type) const
{
    auto filter = std::make_unique<EventFilter>(id, type);
    filter->moveToThread(thread());

    return filter;
}

std::unique_ptr<ActionBuilder> Engine::buildActionBuilder(const api::ActionBuilder& serialized) const
{
    const auto descriptor = actionDescriptor(serialized.type);
    if (!NX_ASSERT(descriptor, "Descriptor for the '%1' type is not registered", serialized.type))
        return {};

    auto builder = buildActionBuilder(serialized.id, serialized.type);
    if (!builder)
        return {};

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
    auto builder = buildActionBuilder(QnUuid::createUuid(), descriptor.id);
    if (!builder)
        return {};

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

std::unique_ptr<ActionBuilder> Engine::buildActionBuilder(QnUuid id, const QString& type) const
{
    ActionConstructor ctor = m_actionTypes.value(type);
    if (!NX_ASSERT(ctor, "Action constructor absent: %1", type))
        return {};

    auto builder = std::make_unique<ActionBuilder>(id, type, ctor);
    builder->moveToThread(thread());
    connect(builder.get(), &ActionBuilder::action, this, &Engine::processAction);

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

size_t Engine::processEvent(const EventPtr& event)
{
    if (!m_enabled)
        return {};

    checkOwnThread();
    NX_DEBUG(this, "Processing Event: %1, state: %2", event->type(), event->state());

    const auto cacheKey = event->cacheKey();
    if (m_eventCache->eventWasCached(cacheKey))
    {
        NX_VERBOSE(this, "Skipping cached event with key: %1", cacheKey);
        return {};
    }

    if (!m_eventCache->checkEventState(event))
        return {};

    EventData eventData;
    QSet<QByteArray> eventFields; // TODO: #spanasenko Cache data.
    QSet<QnUuid> ruleIds, resources;

    {
        NX_MUTEX_LOCKER lock(&m_ruleMutex);

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
    }

    NX_DEBUG(this, "Matched with %1 rules", ruleIds.size());

    if (!ruleIds.empty())
    {
        m_eventCache->cacheEvent(cacheKey);

        eventData = serializeProperties(event.get(), eventFields);
        m_router->routeEvent(eventData, ruleIds, resources);
    }

    return ruleIds.size();
}

size_t Engine::processAnalyticsEvents(const std::vector<EventPtr>& events)
{
    checkOwnThread();

    size_t matchedRules{};
    for (const auto& event: events)
        matchedRules += processEvent(event);

    return matchedRules;
}

// TODO: #spanasenko Use a wrapper with additional checks instead of QHash.
void Engine::processAcceptedEvent(const QnUuid& ruleId, const EventData& eventData)
{
    checkOwnThread();
    NX_DEBUG(this, "Processing accepted event, rule id: %1", ruleId);

    const auto rule = this->rule(ruleId);
    if (!rule)
        return;

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
    checkOwnThread();
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
    checkOwnThread();

    return m_eventCache.get();
}

void Engine::toggleTimer(nx::utils::TimerEventHandler* handler, bool on)
{
    {
        NX_MUTEX_LOCKER lock(&m_ruleMutex);

        if (on)
            m_timerHandlers.insert(handler);
        else
            m_timerHandlers.remove(handler);

        on = !m_timerHandlers.empty();
    }

    executeInThread(
        this->thread(),
        [this, on]()
        {
            if (!on)
                m_aggregationTimer->stop();
            else if (!m_aggregationTimer->isActive())
                m_aggregationTimer->start();
        });
}

void Engine::checkOwnThread() const
{
    const auto currentThread = QThread::currentThread();
    NX_ASSERT(this->thread() == currentThread,
        "Unexpected thread: %1", currentThread->objectName());
}

} // namespace nx::vms::rules
