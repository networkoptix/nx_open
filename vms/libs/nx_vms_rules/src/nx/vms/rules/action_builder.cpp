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
#include "utils/action.h"
#include "utils/field.h"
#include "utils/type.h"

namespace nx::vms::rules {

namespace {

static const QSet<QString> kAllowedEvents = {
    "nx.events.debug",
    "nx.events.test",
};

/** Keep in sync with hasAccessToSource() in the old engine. */
EventPtr permissionFilter(
    const EventPtr& event,
    const QnUserResourcePtr& user,
    const nx::vms::common::SystemContext* context)
{
    const auto manifest = Engine::instance()->eventDescriptor(event->type());
    if (!NX_ASSERT(manifest))
        return {};

    const auto& permissions = manifest->permissions;
    if (permissions.globalPermission != nx::vms::api::GlobalPermission::none
        && !context->resourceAccessManager()
            ->hasGlobalPermission(user, permissions.globalPermission))
    {
        NX_VERBOSE(NX_SCOPE_TAG,
            "User %1 has no global permission %2 for the event %3",
            user, permissions.globalPermission, event->type());

        return {};
    }

    std::map<QByteArray, QnUuidList> filteredFields;
    for (const auto& permission: permissions.resourcePermissions)
    {
        const QVariant value = event->property(permission.fieldName);

        if (value.canConvert<QnUuid>())
        {
            const auto resource = context->resourcePool()->getResourceById(value.value<QnUuid>());

            if (resource && !context->resourceAccessManager()->hasPermission(
                user,
                resource,
                permission.permissions))
            {
                NX_VERBOSE(NX_SCOPE_TAG,
                    "User %1 has no permission for the event %2 with resource %3",
                    user, event->type(), resource);

                return {};
            }
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
                    || context->resourceAccessManager()->hasPermission(
                        user,
                        resource,
                        permission.permissions))
                {
                    filteredResourceIds << resourceId;
                }
            }

            if (filteredResourceIds.isEmpty())
            {
                NX_VERBOSE(NX_SCOPE_TAG,
                    "User %1 has no permissions for any resource of the field %2 of event %3",
                    user, permission.fieldName, event->type());

                return {};
            }

            if (filteredResourceIds.size() != resourceIds.size())
                filteredFields[permission.fieldName] = std::move(filteredResourceIds);
        }
        else
        {
            NX_ASSERT(false, "Unexpected field type: %1", value.typeName());
            return {};
        }
    }

    if (!filteredFields.empty())
    {
        auto clone = Engine::instance()->cloneEvent(event);
        for (const auto& [key, value]: filteredFields)
            clone->setProperty(key, QVariant::fromValue(value));

        return clone;
    }

    return event;
}

QByteArray getUuidsHash(const QnUuidList& ids)
{
    QByteArray result;
    result.reserve(16 * ids.size());

    for(auto id: ids)
        result.push_back(id.toRfc4122());

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
    m_timer.setSingleShot(true);
    // The interval is how often does it required to check if aggregator has events with the elapsed
    // aggregation time.
    constexpr auto kPollAggregatorInterval = seconds(1);
    m_timer.setInterval(kPollAggregatorInterval);
    connect(&m_timer, &QTimer::timeout, this, &ActionBuilder::onTimeout);
}

ActionBuilder::~ActionBuilder()
{
}

QnUuid ActionBuilder::id() const
{
    return m_id;
}

QString ActionBuilder::actionType() const
{
    return m_actionType;
}

QnUuid ActionBuilder::ruleId() const
{
    return m_ruleId;
}

void ActionBuilder::setRuleId(const QnUuid& ruleId)
{
    m_ruleId = ruleId;
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

    if (name == utils::kIntervalFieldName)
    {
        auto optionalTimeField = dynamic_cast<OptionalTimeField*>(field.get());
        if (NX_ASSERT(optionalTimeField))
            setAggregationInterval(optionalTimeField->value());
    }

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
    if (m_aggregator && !m_timer.isActive())
        m_timer.start();

    processEvent(event);
}

void ActionBuilder::processEvent(const EventPtr& event)
{
    if (m_aggregator && m_aggregator->aggregate(event))
    {
        NX_VERBOSE(this, "Event %1 occurred and was aggregated", event->type());
    }
    else
    {
        NX_VERBOSE(this, "Event %1 occurred and was sent to execution", event->type());
        buildAndEmitAction(AggregatedEventPtr::create(event));
    }
}

void ActionBuilder::setAggregationInterval(microseconds interval)
{
    m_interval = interval;
    if (m_interval != microseconds::zero())
    {
        // TODO: #mmalofeev move aggregation key function to the manifest or to some action
        // dependent strategy class when another aggregation logic will be required.
        m_aggregator = QSharedPointer<Aggregator>::create(
            m_interval,
            [](const EventPtr& e) { return e->aggregationKey(); });
    }
}

microseconds ActionBuilder::aggregationInterval() const
{
    return m_interval;
}

bool ActionBuilder::isProlonged() const
{
    return nx::vms::rules::isProlonged(Engine::instance(), this);
}

void ActionBuilder::connectSignals()
{
    for (const auto& [name, field]: m_fields)
    {
        field->connectSignals();
        connect(field.get(), &Field::stateChanged, this, &ActionBuilder::updateState);
    }
}

void ActionBuilder::onTimeout()
{
    if (!NX_ASSERT(m_aggregator) || m_aggregator->empty())
        return;

    m_timer.start();

    handleAggregatedEvents();
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

    Actions actions;

    if (m_fields.contains(utils::kUsersFieldName))
        actions = buildActionsForTargetUsers(aggregatedEvent);
    else
        actions.push_back(buildAction(aggregatedEvent));

    for (const auto& action: actions)
    {
        if(action)
            emit this->action(action);
    }
}

ActionBuilder::Actions ActionBuilder::buildActionsForTargetUsers(
    const AggregatedEventPtr& aggregatedEvent)
{
    auto targetUsersField = fieldByNameImpl<TargetUserField>(utils::kUsersFieldName);
    if (!NX_ASSERT(targetUsersField))
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

        // Checks whether the user has rights to view the event. At the moment only cameras are
        // required to be checked.
        auto filteredAggregatedEvent = aggregatedEvent->filtered(
            [&user, context = targetUsersField->systemContext()](const EventPtr& event)
            {
                return permissionFilter(event, user, context);
            });

        if (!filteredAggregatedEvent)
            continue;

        auto hash = getUuidsHash(utils::getDeviceIds(filteredAggregatedEvent));
        auto it = eventUsersMap.find(hash);

        if (it == eventUsersMap.end())
            eventUsersMap.emplace(hash, EventUsers{filteredAggregatedEvent, {user->getId()}});
        else
            it->second.userIds << user->getId();
    }

    Actions result;

    for (auto& [key, value]: eventUsersMap)
    {
        NX_VERBOSE(this, "Building action for users: %1", value.userIds);

        // Substitute the initial target users with the user the aggregated event has been filtered.
        targetUsersField->setSelection({
            .ids = std::move(value.userIds),
            .all = false});

        result.push_back(buildAction(value.event));
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

    const bool isProlonged = this->isProlonged();
    if (isProlonged &&
        ((m_isActionRunning && aggregatedEvent->state() == State::started)
            || (!m_isActionRunning && aggregatedEvent->state() == State::stopped)))
    {
        return {};
    }

    action->setRuleId(m_ruleId);

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

    // TODO: #amalov The flag is taken from the old engine algorithms.
    // Think of more clever solution to support "Use source" flag.
    if (action && isProlonged)
    {
        m_isActionRunning = (action->state() == State::started);
    }

    return action;
}

void ActionBuilder::handleAggregatedEvents()
{
    for (const auto& aggregationInfo: m_aggregator->popEvents())
        buildAndEmitAction(AggregatedEventPtr::create(aggregationInfo));
}

} // namespace nx::vms::rules
