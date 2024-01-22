// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_builder.h"

#include <QtCore/QJsonValue>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QSet>
#include <QtCore/QVariant>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qobject.h>
#include <nx/vms/common/resource/property_adaptors.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/migration_utils.h>

#include "action_builder_field.h"
#include "action_builder_fields/optional_time_field.h"
#include "action_builder_fields/target_user_field.h"
#include "aggregated_event.h"
#include "aggregator.h"
#include "basic_action.h"
#include "basic_event.h"
#include "engine.h"
#include "rule.h"
#include "utils/action.h"
#include "utils/common.h"
#include "utils/event.h"
#include "utils/field.h"

namespace nx::vms::rules {

namespace {

static const QSet<QString> kAllowedEvents = {
    "nx.events.debug",
    "nx.events.test",
    "nx.events.test.permissions",
};

enum class FiltrationAction
{
    Discard,
    Accept,
    Clone,
};

struct FiltrationResult
{
    FiltrationAction action;
    std::map<std::string, QnUuidList> resourceMap;
    QByteArray hash;
};

// Events or actions may differ by resource list depending on user permissions.
template<class T>
QByteArray permissionHash(const PermissionsDescriptor& permissions, const T& object)
{
    QByteArray result;

    for (const auto& [fieldName, _] : permissions.resourcePermissions)
    {
        const auto property = object->property(fieldName.c_str());
        if (property.template canConvert<QnUuid>())
            result.push_back(property.template value<QnUuid>().toRfc4122());

        if (property.template canConvert<QnUuidList>())
        {
            for (const auto id: property.template value<QnUuidList>())
                result.push_back(id.toRfc4122());
        }
    }

    return result;
}

FiltrationResult filterResourcesByPermission(
    const QObject* object,
    const QString& type,
    const QnUserResourcePtr& user,
    const PermissionsDescriptor& permissionsDescriptor)
{
    if (permissionsDescriptor.empty())
        return {FiltrationAction::Accept};

    const auto context = user->systemContext();
    const auto accessManager = context->resourceAccessManager();
    const auto globalPermission = permissionsDescriptor.globalPermission;

    if ((globalPermission != nx::vms::api::GlobalPermission::none)
        && !accessManager->hasGlobalPermission(user, globalPermission))
    {
        NX_VERBOSE(NX_SCOPE_TAG,
            "User %1 has no global permission %2 for the object %3",
            user, globalPermission, type);

        return {FiltrationAction::Discard};
    }

    auto result = FiltrationResult{FiltrationAction::Clone};
    for (const auto& [fieldName, permissions]: permissionsDescriptor.resourcePermissions)
    {
        const QVariant value = object->property(fieldName.c_str());

        if (value.canConvert<QnUuid>())
        {
            const auto resourceId = value.value<QnUuid>();
            const auto resource = context->resourcePool()->getResourceById(resourceId);

            if (resource &&
                !accessManager->hasPermission(user, resource, permissions))
            {
                NX_VERBOSE(NX_SCOPE_TAG,
                    "User %1 has no permission for the object %2 with resource %3",
                    user, type, resource);

                return {FiltrationAction::Discard};
            }
            result.hash.push_back(resourceId.toRfc4122());
        }
        else if (value.canConvert<QnUuidList>())
        {
            const auto resourceIds = value.value<QnUuidList>();
            if (resourceIds.isEmpty())
                continue;

            QnUuidList filteredResourceIds;

            for (const auto resourceId: resourceIds)
            {
                const auto resource = context->resourcePool()->getResourceById(resourceId);
                if (!resource
                    || accessManager->hasPermission(user, resource, permissions))
                {
                    filteredResourceIds << resourceId;
                }
            }

            if (filteredResourceIds.isEmpty())
            {
                NX_VERBOSE(NX_SCOPE_TAG,
                    "User %1 has no permissions for any resource of the field %2 of object %3",
                    user, fieldName, type);

                return {FiltrationAction::Discard};
            }

            for (const auto id: filteredResourceIds)
                result.hash.push_back(id.toRfc4122());

            if (filteredResourceIds.size() != resourceIds.size())
                result.resourceMap[fieldName] = std::move(filteredResourceIds);
        }
        else
        {
            NX_ASSERT(false, "Unexpected field type: %1", value.typeName());
            return {FiltrationAction::Discard};
        }
    }

    if (!result.resourceMap.empty())
        return result;

    return {FiltrationAction::Accept};
}

EventPtr filterEventPermissions(
    const EventPtr& event,
    const QnUserResourcePtr& user,
    const PermissionsDescriptor& permissionsDescriptor)
{
    auto result = filterResourcesByPermission(event.get(), event->type(), user, permissionsDescriptor);
    if (result.action == FiltrationAction::Accept)
        return event;

    if (result.action == FiltrationAction::Discard)
        return {};

    NX_ASSERT(!result.resourceMap.empty());

    auto clone = user->systemContext()->vmsRulesEngine()->cloneEvent(event);
    for (const auto& [key, value]: result.resourceMap)
        clone->setProperty(key.c_str(), QVariant::fromValue(value));

    return clone;
}

ActionBuilder::Actions filterActionPermissions(
    const ActionPtr& action,
    const nx::vms::common::SystemContext* context,
    const PermissionsDescriptor& permissionsDescriptor)
{
    const auto selection = action->property(utils::kUsersFieldName).value<UuidSelection>();
    NX_ASSERT(!selection.ids.isEmpty());

    struct ActionUsers
    {
        ActionPtr action;
        QnUuidSet userIds;
    };
    std::unordered_map<QByteArray, ActionUsers> actionUsersMap;

    ActionBuilder::Actions result;
    for (const auto& user:
        context->resourcePool()->getResourcesByIds<QnUserResource>(selection.ids))
    {
        auto filterResult =
            filterResourcesByPermission(action.get(), action->type(), user, permissionsDescriptor);

        if (filterResult.action == FiltrationAction::Discard)
            continue;

        if (filterResult.action == FiltrationAction::Accept ||
            filterResult.action == FiltrationAction::Clone)
        {
            if (auto it = actionUsersMap.find(filterResult.hash); it == actionUsersMap.end())
            {
                ActionPtr insert = action;

                if (filterResult.action == FiltrationAction::Clone)
                {
                    insert = user->systemContext()->vmsRulesEngine()->cloneAction(action);
                    for (const auto& [key, value]: filterResult.resourceMap)
                        insert->setProperty(key.c_str(), QVariant::fromValue(value));
                }

                actionUsersMap.emplace(filterResult.hash, ActionUsers{insert, {user->getId()}});
            }
            else
            {
                it->second.userIds << user->getId();
            }
        }
    }

    for (auto& [key, value]: actionUsersMap)
    {
        value.action->setProperty(utils::kUsersFieldName, QVariant::fromValue(UuidSelection{
            .ids = std::move(value.userIds),
            .all = false}));

        result.push_back(std::move(value.action));
    }

    return result;
}

} // namespace

using namespace std::chrono;

ActionBuilder::ActionBuilder(const QnUuid& id, const QString& actionType, const ActionConstructor& ctor):
    m_id(id),
    m_actionType(actionType),
    m_constructor(ctor)
{
    // TODO: #spanasenko Build m_targetFields list.
}

ActionBuilder::~ActionBuilder()
{
    toggleAggregationTimer(false);
}

QnUuid ActionBuilder::id() const
{
    return m_id;
}

QString ActionBuilder::actionType() const
{
    return m_actionType;
}

const Rule* ActionBuilder::rule() const
{
    return m_rule;
}

void ActionBuilder::setRule(const Rule* rule)
{
    m_rule = rule;
}

std::map<QString, QVariant> ActionBuilder::flatData() const
{
    std::map<QString, QVariant> result;
    for (const auto& [id, field]: m_fields)
    {
        const auto& fieldProperties = field->serializedProperties();
        for (auto it = fieldProperties.begin(); it != fieldProperties.end(); ++it)
        {
            const QString path = id + "/" + it.key();
            result[path] = it.value();
        }
    }
    return result;
}

bool ActionBuilder::updateData(const QString& path, const QVariant& value)
{
    const auto ids = path.split('/');
    if (ids.size() != 2)
        return false;
    if (m_fields.find(ids[0]) == m_fields.end())
        return false;

    m_fields[ids[0]]->setProperty(ids[1].toLatin1().data(), value);
    return true;
}

bool ActionBuilder::updateFlatData(const std::map<QString, QVariant>& data)
{
    std::vector<std::tuple<QString, QString, QVariant>> parsed;

    /* Check data. */
    for (const auto& [id, value]: data)
    {
        const auto ids = id.split('/');
        if (ids.size() != 2)
            return false;
        if (m_fields.find(ids[0]) == m_fields.end())
         return false;
        parsed.push_back({ids[0], ids[1], value});
    }

    /* Update. */
    for (const auto& [field, prop, value]: parsed)
    {
        m_fields[field]->setProperty(prop.toLatin1().data(), value);
    }
    return true;
}

void ActionBuilder::addField(const QString& name, std::unique_ptr<ActionBuilderField> field)
{
    if (!NX_ASSERT(field))
        return;

    field->moveToThread(thread());

    if (name == utils::kIntervalFieldName)
    {
        auto optionalTimeField = dynamic_cast<OptionalTimeField*>(field.get());
        if (NX_ASSERT(optionalTimeField))
            setAggregationInterval(optionalTimeField->value());
    }

    static const auto onFieldChangedMetaMethod =
        metaObject()->method(metaObject()->indexOfMethod("onFieldChanged()"));
    if (NX_ASSERT(onFieldChangedMetaMethod.isValid()))
        nx::utils::watchOnPropertyChanges(field.get(), this, onFieldChangedMetaMethod);

    m_fields[name] = std::move(field);

    updateState();
}

const QHash<QString, ActionBuilderField*> ActionBuilder::fields() const
{
    QHash<QString, ActionBuilderField*> result;
    for (const auto& [name, field]: m_fields)
    {
        result[name] = field.get();
    }
    return result;
}

QSet<QString> ActionBuilder::requiredEventFields() const
{
    QSet<QString> result;
    for (const auto& [name, field]: m_fields)
    {
        result += field->requiredEventFields();
    }
    return result;
}

QSet<QnUuid> ActionBuilder::affectedResources(const EventPtr& /*event*/) const
{
    NX_ASSERT(false, "Not implemented");
    return {};
}

void ActionBuilder::process(const EventPtr& event)
{
    if (m_aggregator)
        toggleAggregationTimer(true);

    processEvent(event);
}

void ActionBuilder::processEvent(const EventPtr& event)
{
    const auto actionDescriptor = engine()->actionDescriptor(m_actionType);
    const auto eventDescriptor = engine()->eventDescriptor(event->type());
    if (!NX_ASSERT(eventDescriptor) || !NX_ASSERT(actionDescriptor))
        return;

    const auto isEventProlonged = rules::isProlonged(event); //< Whether event state is 'started' or 'stopped'.
    const auto isActionProlonged = isProlonged(); //< Whether action has 'prolonged' flag and duration is not set.
    if (isActionProlonged && !isEventProlonged)
    {
        // Prolonged action without fixed duration must be started and stopped according to the
        // event state. As instant event does not have such states and not supported.
        NX_ASSERT(false, "Only prolonged events are supported by the prolonged actions");
        return;
    }

    const bool aggregateByType = utils::aggregateByType(*eventDescriptor, *actionDescriptor);

    static const auto aggregationKey = [](const EventPtr& e) { return e->aggregationKey(); };
    static const auto eventType = [](const EventPtr& e) { return e->type(); };

    if (!isActionProlonged //< Only events for instant or fixed duration action might be aggregated.
        && m_aggregator //< Action has 'interval' field and interval value more than zero.
        && m_aggregator->aggregate(
            event, (aggregateByType ? eventType : aggregationKey)))
    {
        NX_VERBOSE(this, "Event %1(%2) occurred and was aggregated", event->type(), event->state());
    }
    else
    {
        NX_VERBOSE(this, "Event %1(%2) occurred and was sent to execution", event->type(), event->state());
        buildAndEmitAction(AggregatedEventPtr::create(event));
    }
}

void ActionBuilder::setAggregationInterval(microseconds interval)
{
    m_interval = interval;
    if (m_interval != microseconds::zero())
        m_aggregator = QSharedPointer<Aggregator>::create(m_interval);
}

microseconds ActionBuilder::aggregationInterval() const
{
    return m_interval;
}

void ActionBuilder::toggleAggregationTimer(bool on)
{
    if (on == m_timerActive || !engine())
        return;

    engine()->toggleTimer(this, on);
    m_timerActive = on;
}

bool ActionBuilder::isProlonged() const
{
    if (const auto engine = this->engine(); NX_ASSERT(engine))
        return nx::vms::rules::isProlonged(engine, this);

    return {};
}

void ActionBuilder::connectSignals()
{
    for (const auto& [name, field]: m_fields)
    {
        field->connectSignals();
        connect(field.get(), &Field::stateChanged, this, &ActionBuilder::updateState);
    }
}

void ActionBuilder::onTimer(const nx::utils::TimerId&)
{
    if (!NX_ASSERT(m_aggregator) || !NX_ASSERT(m_timerActive) || m_aggregator->empty())
    {
        toggleAggregationTimer(false);
    }
    else
    {
        toggleAggregationTimer(true);
        handleAggregatedEvents();
    }
}

void ActionBuilder::updateState()
{
    //TODO: #spanasenko Update derived values (error messages, etc.)

    if (m_updateInProgress)
        return;

    QScopedValueRollback<bool> guard(m_updateInProgress, true);
    emit stateChanged();
}

void ActionBuilder::buildAndEmitAction(const AggregatedEventPtr& aggregatedEvent)
{
    if (!NX_ASSERT(m_constructor) || !NX_ASSERT(aggregatedEvent))
        return;

    const bool isProlonged = this->isProlonged();
    if (isProlonged &&
        ((m_isActionRunning && aggregatedEvent->state() == State::started)
            || (!m_isActionRunning && aggregatedEvent->state() == State::stopped)))
    {
        return;
    }

    // Action with all data without any user filtration for logging.
    ActionPtr logAction = buildAction(aggregatedEvent);
    if (!NX_ASSERT(logAction))
        return;

    Actions actions;

    if (m_fields.contains(utils::kUsersFieldName))
    {
        actions = buildActionsForTargetUsers(aggregatedEvent);

        if (engine()->actionDescriptor(actionType())->flags
            .testFlag(ItemFlag::executeOnClientAndServer))
        {
            // Action with all data, but without users for server processing.
            auto serverAction = engine()->cloneAction(logAction);
            serverAction->setProperty(utils::kUsersFieldName, {});
            actions.push_back(serverAction);
        }
    }
    else
    {
        actions.push_back(logAction);
    }

    std::erase(actions, ActionPtr());
    if (actions.empty())
        return;

    // TODO: #amalov The flag is taken from the old engine algorithms.
    // Think of more clever solution to support "Use source" flag.
    if (isProlonged)
        m_isActionRunning = (actions.front()->state() == State::started);

    for (const auto& action: actions)
        emit this->action(action);

    emit engine()->actionBuilt(aggregatedEvent, logAction);
}

ActionBuilder::Actions ActionBuilder::buildActionsForTargetUsers(
    const AggregatedEventPtr& aggregatedEvent)
{
    auto targetUsersField = fieldByNameImpl<TargetUserField>(utils::kUsersFieldName);
    if (!NX_ASSERT(targetUsersField))
        return {};

    const auto eventManifest = engine()->eventDescriptor(aggregatedEvent->type());
    if (!NX_ASSERT(eventManifest))
        return {};

    UuidSelection initialFieldValue {
        .ids = targetUsersField->ids(),
        .all = targetUsersField->acceptAll()
    };

    struct EventUsers
    {
        AggregatedEventPtr event;
        QnUuidSet userIds;
    };

    std::unordered_map<QByteArray, EventUsers> eventUsersMap;
    nx::vms::common::BusinessEventFilterResourcePropertyAdaptor userFilter;

    QSignalBlocker signalBlocker{targetUsersField};

    // If the action should be shown to some users it is required to check if the user has
    // appropriate rights to see the event details.
    for (const auto& user: targetUsersField->users())
    {
        if (!kAllowedEvents.contains(aggregatedEvent->type()))
        {
            userFilter.setResource(user);
            if (!userFilter.isAllowed(nx::vms::event::convertEventType(aggregatedEvent->type())))
            {
                NX_VERBOSE(this, "Event %1 is filtered by user %2",
                    aggregatedEvent->type(), user->getName());
                continue;
            }
        }

        auto filteredAggregatedEvent = aggregatedEvent->filtered(
            [&user, &permissions = eventManifest->permissions](const EventPtr& event)
            {
                return filterEventPermissions(event, user, permissions);
            });

        if (!filteredAggregatedEvent)
            continue;

        // Grouping users with the same permissions.
        auto hash = permissionHash(eventManifest->permissions, filteredAggregatedEvent);
        auto it = eventUsersMap.find(hash);

        if (it == eventUsersMap.end())
            eventUsersMap.emplace(hash, EventUsers{filteredAggregatedEvent, {user->getId()}});
        else
            it->second.userIds << user->getId();
    }

    Actions result;
    auto actionManifect = engine()->actionDescriptor(actionType());

    for (auto& [key, value]: eventUsersMap)
    {
        NX_VERBOSE(this, "Building action for users: %1", value.userIds);

        // Substitute the initial target users with the user the aggregated event has been filtered.
        targetUsersField->setSelection({
            .ids = std::move(value.userIds),
            .all = false});

        auto actions = filterActionPermissions(
            buildAction(value.event),
            targetUsersField->systemContext(),
            actionManifect->permissions);

        result.insert(result.end(), actions.begin(), actions.end());
    }

    // Recover initial target users selection.
    targetUsersField->setSelection(initialFieldValue);

    return result;
}

ActionPtr ActionBuilder::buildAction(const AggregatedEventPtr& aggregatedEvent)
{
    ActionPtr action(m_constructor());
    if (!action)
        return {};

    action->setRuleId(m_rule ? m_rule->id() : QnUuid());

    const auto propertyNames =
        nx::utils::propertyNames(action.get(), nx::utils::PropertyAccess::writable);
    for (const auto& propertyName: propertyNames)
    {
        if (m_fields.contains(propertyName))
        {
            auto& field = m_fields.at(propertyName);
            const auto value = field->build(aggregatedEvent);
            action->setProperty(propertyName, value);
        }
        else
        {
            // Set property value only if it exists.
            if (action->property(propertyName).isValid()
                && aggregatedEvent->property(propertyName).isValid())
            {
                action->setProperty(propertyName, aggregatedEvent->property(propertyName));
            }
        }
    }

    return action;
}

void ActionBuilder::handleAggregatedEvents()
{
    for (const auto& event: m_aggregator->popEvents())
    {
        NX_VERBOSE(
            this, "Aggregated event %1(%2) was sent to execution", event->type(), event->state());
        buildAndEmitAction(event);
    }
}

Engine* ActionBuilder::engine() const
{
    return m_rule ? const_cast<Engine*>(m_rule->engine()) : nullptr;
}

void ActionBuilder::onFieldChanged()
{
    const auto changedField = sender();

    for (const auto& [fieldName, field]: m_fields)
    {
        if (field.get() == changedField)
        {
            emit changed(fieldName);
            return;
        }
    }

    NX_ASSERT(false, "Must not be here");
}

} // namespace nx::vms::rules
