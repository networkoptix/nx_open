// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <chrono>

#include <QtCore/QMetaProperty>
#include <QtCore/QThread>

#include <nx/fusion/serialization/json.h>
#include <nx/utils/i18n/scoped_locale.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qobject.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/utils/schedule.h>
#include <nx/vms/rules/ini.h>
#include <nx/vms/rules/utils/action.h>
#include <nx/vms/rules/utils/event.h>
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
#include "utils/resource.h"
#include "utils/serialization.h"
#include "utils/type.h"

namespace nx::vms::rules {

using namespace std::chrono;

namespace {

// TODO: #amalov Extend this check with BuiltinTypesTest checks for external manifest registration.
bool isDescriptorValid(const ItemDescriptor& descriptor)
{
    if (descriptor.id.isEmpty() || descriptor.displayName.empty())
        return false;

    std::set<QString> uniqueFields;
    for (const auto& field: descriptor.fields)
    {
        if (!uniqueFields.insert(field.fieldName).second)
            return false;
    }

    return true;
}

size_t qHash(const vms::rules::ActionPtr& action)
{
    UuidList ids = utils::getResourceIds(action);

    const auto users = //< TODO: #mmalofeev remove when https://networkoptix.atlassian.net/browse/VMS-38891 will be done.
        utils::getFieldValue<vms::rules::UuidSelection>(action, utils::kUsersFieldName);
    ids << UuidList{users.ids.cbegin(), users.ids.constEnd()};

    return qHash(ids);
}

} // namespace

Engine::Engine(
    common::SystemContext* systemContext,
    std::unique_ptr<Router> router,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext),
    m_router(std::move(router)),
    m_ruleMutex(nx::Mutex::Recursive),
    m_aggregationTimer(new QTimer(this))
{
    systemContext->setVmsRulesEngine(this);

    setEnabledFieldsFromIni();

    m_aggregationTimer->setInterval(1s);
    connect(m_aggregationTimer, &QTimer::timeout, this,
        [this]()
        {
            NX_MUTEX_LOCKER lock(&m_ruleMutex);

            for (auto handler: m_timerHandlers)
                handler->onTimer(0);
        });

    if (m_router) //< Router may absent in test scenarios.
    {
        connect(m_router.get(), &Router::eventReceived, this, &Engine::onEventReceved);
        connect(m_router.get(), &Router::actionReceived, this, &Engine::processAcceptedAction);
    }

    connect(this, &Engine::ruleAddedOrUpdated, this,
        [this](nx::Uuid ruleId, bool added)
        {
            if (const auto rule = this->rule(ruleId); !added && rule && !rule->enabled())
                    stopRunningActions(ruleId); //< The rule was disabled.
        });

    connect(
        this,
        &Engine::ruleRemoved,
        this,
        [this](nx::Uuid ruleId)
        {
            stopRunningActions(ruleId);
        });

    connect(
        this,
        &Engine::rulesReset,
        this,
        [this]
        {
            for (const auto ruleId: m_runningRules.keys())
                stopRunningActions(ruleId);
        }
    );
}

Engine::~Engine()
{
    NX_MUTEX_LOCKER lock(&m_ruleMutex);
    m_rules.clear();
}

Router* Engine::router() const
{
    return m_router.get();
}

void Engine::setEnabledFieldsFromIni()
{
    const QString rulesVersion(ini().rulesEngine);
    m_enabled = (rulesVersion == "new" || rulesVersion == "both");
    m_oldEngineEnabled = (rulesVersion != "new");
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

ConstRulePtr Engine::rule(nx::Uuid id) const
{
    NX_MUTEX_LOCKER lock(&m_ruleMutex);
    if (const auto it = m_rules.find(id); it != m_rules.end())
        return it->second;

    return {};
}

RulePtr Engine::cloneRule(nx::Uuid id) const
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

void Engine::removeRule(nx::Uuid ruleId)
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
        NX_ERROR(this, "Register event failed: %1 is already registered", descriptor.id);
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

    auto result = defaultEventGroups(systemContext());

    auto visitor =
        [&eventByGroup](Group& group, auto visitor)->void
        {
            group.items = eventByGroup.value(group.id);
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

    if (descriptor.flags.testFlags({ItemFlag::instant, ItemFlag::prolonged}))
    {
        NX_ERROR(this, "Action cannot be instant and prolonged simultaneously.");
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
    if (isEventFieldRegistered(type))
        return false;

    m_eventFields[type] = ctor;
    return true;
}

bool Engine::registerActionField(const QString& type, const ActionFieldRecord& record)
{
    if (isActionFieldRegistered(type))
        return false;

    m_actionFields.insert(type, record);
    return true;
}

const QSet<QString>& Engine::encryptedActionBuilderProperties(const QString& type) const
{
    static const QSet<QString> kEmpty;

    const auto it = m_actionFields.find(type);
    return NX_ASSERT(it != m_actionFields.end(), "Unregistered field type: %1", type)
        ? it.value().encryptedFields
        : kEmpty;
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
    rule->setInternal(serialized.internal);
    rule->setSchedule(nx::vms::common::scheduleToByteArray(serialized.schedule));

    return rule;
}

EventPtr Engine::buildEvent(const EventData& eventData) const
{
    const QString eventType = eventData.value(utils::kType).toString();
    if (!NX_ASSERT(m_eventTypes.contains(eventType), "Unknown event type: %1", eventType))
        return {};

    auto event = EventPtr(m_eventTypes[eventType]());
    if (deserializeProperties(eventData, event.get()))
        return event;

    return {};
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
            field = buildEventField(serializedFieldIt->second, &fieldDescriptor);
        }
        else //< As the field isn't present in the received serialized filter, make a default one.
        {
            field = buildEventField(&fieldDescriptor);
            field->setProperties(fieldDescriptor.properties);
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
        auto field = buildEventField(&fieldDescriptor);
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

std::unique_ptr<EventFilter> Engine::buildEventFilter(nx::Uuid id, const QString& type) const
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
            field = buildActionField(serializedFieldIt->second, &fieldDescriptor);
        }
        else //< As the field isn't present in the received serialized builder, make a default one.
        {
            field = buildActionField(&fieldDescriptor);
            field->setProperties(fieldDescriptor.properties);
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

bool Engine::registerValidator(const QString& fieldType, FieldValidator* validator)
{
    if (!NX_ASSERT(!fieldType.isEmpty()) || !NX_ASSERT(validator))
        return false;

    validator->setParent(this); //< TODO: #mmalofeev use unique_ptr instead?
    m_fieldValidators.insert(fieldType, validator);

    return true;
}

FieldValidator* Engine::fieldValidator(const QString& fieldType) const
{
    auto fieldValidatorIt = m_fieldValidators.find(fieldType);
    return fieldValidatorIt == m_fieldValidators.cend() ? nullptr : fieldValidatorIt.value();
}

std::unique_ptr<ActionBuilder> Engine::buildActionBuilder(const ItemDescriptor& descriptor) const
{
    auto builder = buildActionBuilder(nx::Uuid::createUuid(), descriptor.id);
    if (!builder)
        return {};

    for (const auto& fieldDescriptor: descriptor.fields)
    {
        auto field = buildActionField(&fieldDescriptor);
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

std::unique_ptr<ActionBuilder> Engine::buildActionBuilder(nx::Uuid id, const QString& type) const
{
    ActionConstructor ctor = m_actionTypes.value(type);
    if (!NX_ASSERT(ctor, "Action constructor absent: %1", type))
        return {};

    auto builder = std::make_unique<ActionBuilder>(id, type, ctor);
    builder->moveToThread(thread());
    connect(builder.get(), &ActionBuilder::action, this, &Engine::processAction);

    return builder;
}

std::unique_ptr<EventFilterField> Engine::buildEventField(
    const api::Field& serialized, const FieldDescriptor* descriptor) const
{
    auto field = buildEventField(descriptor);
    if (!field)
        return nullptr;

    deserializeProperties(serialized.props, field.get());

    return field;
}

std::unique_ptr<EventFilterField> Engine::buildEventField(
    const FieldDescriptor* fieldDescriptor) const
{
    const auto& constructor = m_eventFields.value(fieldDescriptor->id);
    if (!constructor)
    {
        NX_ERROR(
            this,
            "Failed to build event field as an event field for the %1 type is not registered",
            fieldDescriptor->id);

        return nullptr;
    }

    std::unique_ptr<EventFilterField> field(constructor(fieldDescriptor));

    if (NX_ASSERT(field, "Field constructor returns nullptr: %1", fieldDescriptor->id))
        field->moveToThread(thread());

    return field;
}

std::unique_ptr<ActionBuilderField> Engine::buildActionField(
    const api::Field& serialized, const FieldDescriptor* descriptor) const
{
    auto field = buildActionField(descriptor);
    deserializeProperties(serialized.props, field.get());

    return field;
}

std::unique_ptr<ActionBuilderField> Engine::buildActionField(
    const FieldDescriptor* descriptor) const
{
    const auto it = m_actionFields.find(descriptor->id);
    if (it == m_actionFields.end())
    {
        NX_ERROR(
            this,
            "Failed to build action field as an action field for the %1 type is not registered",
            descriptor->id);

        return {};
    }

    std::unique_ptr<ActionBuilderField> field(it->constructor(descriptor));

    if (NX_ASSERT(field, "Field constructor returns nullptr: %1", descriptor->id))
        field->moveToThread(thread());

    return field;
}

size_t Engine::processEvent(const EventPtr& event)
{
    if (!m_enabled)
        return 0;

    checkOwnThread();
    NX_VERBOSE(this, "Processing Event: %1, state: %2", event->type(), event->state());

    std::vector<ConstRulePtr> triggeredRules;
    std::vector<nx::Uuid> runningRules;

    {
        NX_MUTEX_LOCKER lock(&m_ruleMutex);

        // TODO: #spanasenko Add filters-by-type maps?
        for (const auto& [id, rule]: m_rules)
        {
            if (!rule->enabled() || !rule->isValid())
                continue;

            auto cacheKey = event->cacheKey();
            if (!cacheKey.isEmpty())
            {
                cacheKey += id.toString();

                m_eventCache.rememberEvent(cacheKey);
                if (m_eventCache.isReportedRecently(cacheKey))
                {
                    NX_VERBOSE(this, "Skipping cached event with key: %1", cacheKey);
                    continue;
                }
            }

            // Check if the event is matched by field filters except state.
            auto matchedFilters = rule->eventFiltersByType(event->type());
            matchedFilters.removeIf([&event](auto filter){ return !filter->matchFields(event); });

            if (matchedFilters.empty())
                continue;

            runningRules.push_back(id);

            if (!checkRunningEvent(id, event))
                matchedFilters.clear();

            matchedFilters.removeIf([&event](auto filter){ return !filter->matchState(event); });

            // TODO: 1. use cachedEventData to match AnalyticsObject more correctly
            // (bestShot doesn't has typeId, it is needed to remember at context and pass context to the matcher).
            // TODO: 2. Use addition filtering to not repeat Event for the same objectTrack (same old engine).

            if (!matchedFilters.empty())
            {
                triggeredRules.push_back(rule);
                if (!cacheKey.isEmpty())
                    m_eventCache.reportEvent(cacheKey);
            }
        }
    }

    NX_DEBUG(this, "Matched with %1 rules", triggeredRules.size());

    if (!triggeredRules.empty())
        m_router->routeEvent(event, triggeredRules);

    if (event->state() == State::stopped)
    {
        for (const auto& id: runningRules)
            RunningEventWatcher(m_runningRules[id]).erase(event);
    }

    return triggeredRules.size();
}

size_t Engine::processAnalyticsEvents(const std::vector<EventPtr>& events)
{
    checkOwnThread();

    size_t matchedRules{};
    for (const auto& event: events)
        matchedRules += processEvent(event);

    return matchedRules;
}

void Engine::onEventReceved(const EventPtr& event, const std::vector<ConstRulePtr>& triggeredRules)
{
    for (const auto& rule: triggeredRules)
        processAcceptedEvent(event, rule);
}

void Engine::processAcceptedEvent(const EventPtr& event, const ConstRulePtr& rule)
{
    checkOwnThread();
    NX_DEBUG(this, "Processing accepted event: %1, rule: %2", event->type(), rule->id());

    if (!rule->timeInSchedule(qnSyncTime->currentDateTime()))
    {
        NX_VERBOSE(this, "Time is not in rule schedule, skipping the event.");
        return;
    }

    for (const auto builder: rule->actionBuilders())
    {
        nx::i18n::ScopedLocalePtr scopedLocale;

        if (auto executor = m_executors.value(builder->actionType()))
            scopedLocale = executor->translateAction(builder->actionType());

        builder->process(event);
    }
}

void Engine::processAction(const ActionPtr& action) const
{
    checkOwnThread();
    NX_DEBUG(this, "Routing action: %1, state: %2", action->type(), action->state());

    m_router->routeAction(action);
}

void Engine::processAcceptedAction(const ActionPtr& action)
{
    checkOwnThread();

    NX_DEBUG(this, "Processing accepted action: %1, rule : %2", action->type(), action->ruleId());

    if (auto executor = m_executors.value(action->type()))
    {
        const auto isActionProlonged = isProlonged(action);
        if (isActionProlonged && action->state() == State::started)
            m_runningRules[action->ruleId()].actions.insert(qHash(action), action);

        executor->execute(action);

        if (isActionProlonged && action->state() == State::stopped)
        {
            auto runningRuleIt = m_runningRules.find(action->ruleId());
            if (runningRuleIt != m_runningRules.end())
            {
                runningRuleIt->actions.remove(qHash(action));

                if (runningRuleIt->actions.empty())
                    m_runningRules.erase(runningRuleIt);
            }
        }
    }
}

bool Engine::isEventFieldRegistered(const QString& fieldId) const
{
    return m_eventFields.contains(fieldId);
}

bool Engine::isActionFieldRegistered(const QString& fieldId) const
{
    return m_actionFields.contains(fieldId);
}

EventCache* Engine::eventCache()
{
    checkOwnThread();

    return &m_eventCache;
}

RunningEventWatcher Engine::runningEventWatcher(nx::Uuid ruleId)
{
    checkOwnThread();

    return RunningEventWatcher(m_runningRules[ruleId]);
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

bool Engine::checkRunningEvent(nx::Uuid ruleId, const EventPtr& event)
{
    if (!isProlonged(event))
        return true;

    auto runningEventWatcher = RunningEventWatcher(m_runningRules[ruleId]);

    // It is required to check if the prolonged events are coming in the right order.
    // Each stopped event must follow started event. Repeated start and stop events
    // should be ignored.
    const auto isEventRunning = runningEventWatcher.isRunning(event);

    if (event->state() == State::started)
    {
        if (isEventRunning)
        {
            NX_VERBOSE(this, "Event %1-%2 doesn't match due to repeated start",
                event->type(), event->resourceKey());
            return false;
        }

        // Appropriate `stopped` event must be removed from the watcher when the the engine
        // completely process the event - after all the action builders are called.
        runningEventWatcher.add(event);
    }
    else // State::stopped
    {
        if (!isEventRunning)
        {
            NX_VERBOSE(this, "Event %1-%2 doesn't match due to stop without start",
                event->type(), event->resourceKey());
            return false;
        }

        // Only last running stop event should produce stop action.
        if (runningEventWatcher.runningResourceCount() > 1)
        {
            NX_VERBOSE(this, "Event %1-%2 will not stop action since other events are running.",
                event->type(), event->resourceKey());

            return false;
        }
    }

    return true;
}

void Engine::stopRunningActions(nx::Uuid ruleId)
{
    checkOwnThread();

    auto runningRuleIt = m_runningRules.find(ruleId);
    if (runningRuleIt == m_runningRules.end())
        return;

    for (const auto& action: runningRuleIt->actions.values())
        stopRunningAction(action);
}

void Engine::stopRunningAction(const ActionPtr& action)
{
    checkOwnThread();

    NX_VERBOSE(this, "Stop running '%1' action", action->type());

    action->setState(State::stopped);
    action->setTimestamp(qnSyncTime->currentTimePoint());

    processAcceptedAction(action); //< TODO: #mmalofeev should it be written to the event log?
}

} // namespace nx::vms::rules
