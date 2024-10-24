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
#include <nx/utils/i18n/scoped_locale.h>
#include <nx/utils/i18n/translation_manager.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qobject.h>
#include <nx/vms/common/application_context.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/migration_utils.h>

#include "action_builder_field.h"
#include "action_builder_fields/optional_time_field.h"
#include "action_builder_fields/target_users_field.h"
#include "action_builder_fields/text_field.h"
#include "aggregated_event.h"
#include "aggregator.h"
#include "basic_action.h"
#include "basic_event.h"
#include "engine.h"
#include "router.h"
#include "rule.h"
#include "utils/action.h"
#include "utils/common.h"
#include "utils/event.h"
#include "utils/field.h"

namespace nx::vms::rules {

namespace {

enum class FiltrationAction
{
    Discard,
    Accept,
    Clone,
};

struct FiltrationResult
{
    FiltrationAction action;
    std::map<std::string, UuidList> resourceMap;
    QByteArray hash;
};

// Events or actions may differ by resource list depending on user permissions.
template<class T>
QByteArray permissionHash(const ItemDescriptor& manifest, const T& object)
{
    QByteArray result;

    for (const auto& [fieldName, _]: manifest.resources)
    {
        const auto property = object->property(fieldName.c_str());
        if (property.template canConvert<nx::Uuid>())
            result.push_back(property.template value<nx::Uuid>().toRfc4122());

        if (property.template canConvert<UuidList>())
        {
            for (const auto id: property.template value<UuidList>())
                result.push_back(id.toRfc4122());
        }
    }

    return result;
}

bool requireReadPermissions(const ItemDescriptor& manifest)
{
    if (manifest.readPermissions)
        return true;

    for (const auto& [_, desc]: manifest.resources)
    {
        if (desc.readPermissions)
            return true;
    }

    return false;
}

FiltrationResult filterResourcesByPermission(
    const QObject* object,
    const QString& type,
    const QnUserResourcePtr& user,
    const ItemDescriptor& manifest)
{
    if (!requireReadPermissions(manifest))
        return {FiltrationAction::Accept};

    const auto context = user->systemContext();
    const auto accessManager = context->resourceAccessManager();
    const auto globalPermissions = manifest.readPermissions;

    if ((globalPermissions != nx::vms::api::GlobalPermission::none)
        && !accessManager->globalPermissions(user).testFlags(globalPermissions))
    {
        NX_VERBOSE(NX_SCOPE_TAG,
            "User %1 has no global permissions %2 for the object %3",
            user, globalPermissions, type);

        return {FiltrationAction::Discard};
    }

    auto result = FiltrationResult{FiltrationAction::Clone};
    for (const auto& [fieldName, descriptor]: manifest.resources)
    {
        const QVariant value = object->property(fieldName.c_str());

        if (value.canConvert<nx::Uuid>())
        {
            const auto resourceId = value.value<nx::Uuid>();
            const auto resource = context->resourcePool()->getResourceById(resourceId);

            if (resource &&
                !accessManager->hasPermission(user, resource, descriptor.readPermissions))
            {
                NX_VERBOSE(NX_SCOPE_TAG,
                    "User %1 has no permission for the object %2 with resource %3",
                    user, type, resource);

                return {FiltrationAction::Discard};
            }
            result.hash.push_back(resourceId.toRfc4122());
        }
        else if (value.canConvert<UuidList>())
        {
            const auto resourceIds = value.value<UuidList>();
            if (resourceIds.isEmpty())
                continue;

            UuidList filteredResourceIds;

            for (const auto resourceId: resourceIds)
            {
                const auto resource = context->resourcePool()->getResourceById(resourceId);
                if (!resource
                    || accessManager->hasPermission(user, resource, descriptor.readPermissions))
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
    const ItemDescriptor& manifest)
{
    auto result = filterResourcesByPermission(event.get(), event->type(), user, manifest);
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
    const ItemDescriptor& manifest)
{
    const auto selection = action->property(utils::kUsersFieldName).value<UuidSelection>();
    NX_ASSERT(!selection.ids.isEmpty());

    struct ActionUsers
    {
        ActionPtr action;
        UuidSet userIds;
    };
    std::unordered_map<QByteArray, ActionUsers> actionUsersMap;

    ActionBuilder::Actions result;
    for (const auto& user:
        context->resourcePool()->getResourcesByIds<QnUserResource>(selection.ids))
    {
        auto filterResult =
            filterResourcesByPermission(action.get(), action->type(), user, manifest);

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

bool ActionBuilder::bypassScopedLocaleForTest = false;

using namespace std::chrono;

ActionBuilder::ActionBuilder(
    nx::Uuid id, const QString& actionType, const ActionConstructor& ctor)
    :
    m_id(id),
    m_actionType(actionType),
    m_constructor(ctor)
{
    NX_CRITICAL(ctor);
}

ActionBuilder::~ActionBuilder()
{
    toggleAggregationTimer(false);
}

nx::Uuid ActionBuilder::id() const
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

    const auto fieldPtr = field.get();
    m_fields[name] = std::move(field);

    auto appContext = nx::vms::common::appContext();
    if (appContext && nx::vms::api::PeerData::isClient(appContext->localPeerType()))
    {
        static const auto onFieldChangedMetaMethod =
            metaObject()->method(metaObject()->indexOfMethod("onFieldChanged()"));
        if (NX_ASSERT(onFieldChangedMetaMethod.isValid()))
            nx::utils::watchOnPropertyChanges(fieldPtr, this, onFieldChangedMetaMethod);
    }
}

QHash<QString, ActionBuilderField*> ActionBuilder::fields() const
{
    QHash<QString, ActionBuilderField*> result;
    for (const auto& [name, field]: m_fields)
    {
        result[name] = field.get();
    }
    return result;
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
    return nx::vms::rules::isProlonged(engine(), this);
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

void ActionBuilder::buildAndEmitAction(const AggregatedEventPtr& aggregatedEvent)
{
    aggregatedEvent->setRuleId(rule()->id());

    // Action with all data without any user filtration for logging.
    ActionPtr logAction = buildAction(aggregatedEvent);
    if (!NX_ASSERT(logAction))
        return;

    logAction->setId(nx::Uuid::createUuid());

    Actions actions;
    const auto manifest = engine()->actionDescriptor(actionType());

    if (manifest->executionTargets.testFlag(ExecutionTarget::clients))
        actions = buildActionsForTargetUsers(aggregatedEvent);

    if (manifest->executionTargets.testFlag(ExecutionTarget::servers))
    {
        auto serverAction = engine()->cloneAction(logAction);
        // Target users should only be processed if they are not processed on the client side.
        if (manifest->executionTargets.testFlag(ExecutionTarget::clients))
            serverAction->setProperty(utils::kUsersFieldName, {});
        actions.push_back(serverAction);
    }

    std::erase(actions, ActionPtr());
    if (actions.empty())
        return;

    for (auto& action: actions)
        action->setId(logAction->id());

    for (const auto& action: actions)
        emit this->action(action);

    emit engine()->actionBuilt(aggregatedEvent, logAction);
}

ActionBuilder::Actions ActionBuilder::buildActionsForTargetUsers(
    const AggregatedEventPtr& aggregatedEvent)
{
    auto targetUsersField = fieldByNameImpl<TargetUsersField>(utils::kUsersFieldName);
    if (!NX_ASSERT(targetUsersField))
        return {};

    const auto eventManifest = engine()->eventDescriptor(aggregatedEvent->type());
    if (!NX_ASSERT(eventManifest))
        return {};

    const auto actionManifest = engine()->actionDescriptor(actionType());

    UuidSelection initialTargetUsersValue {
        .ids = targetUsersField->ids(),
        .all = targetUsersField->acceptAll()
    };

    struct EventUsers
    {
        AggregatedEventPtr event;
        UuidSet userIds;
        QString locale;
    };

    // Group users by permissions and locale, so only limited number of actions will be produced.
    std::unordered_map<QByteArray, EventUsers> eventUsersMap;
    const bool userFiltered = actionManifest->flags.testFlag(ItemFlag::userFiltered);

    // If the action should be shown to some users it is required to check if the user has
    // appropriate rights to see the event details.
    for (const auto& user: targetUsersField->users())
    {
        if (userFiltered && !user->settings().isEventWatched(aggregatedEvent->type()))
        {
            NX_VERBOSE(this, "Event %1 is filtered by user %2",
                aggregatedEvent->type(), user->getName());
            continue;
        }

        auto filteredAggregatedEvent = aggregatedEvent->filtered(
            [&user, &eventManifest](const EventPtr& event)
            {
                return filterEventPermissions(event, user, *eventManifest);
            });

        if (!filteredAggregatedEvent)
            continue;

        const auto locale = user->locale();
        // Grouping users with the same permissions and same locale.
        auto hash = permissionHash(*eventManifest, filteredAggregatedEvent)
            .append(locale.toUtf8());

        auto it = eventUsersMap.find(hash);

        if (it == eventUsersMap.end())
        {
            eventUsersMap.emplace(hash, EventUsers{
                filteredAggregatedEvent,
                {user->getId()},
                locale
            });
        }
        else
        {
            it->second.userIds << user->getId();
        }
    }

    auto additionalRecipientsField = fieldByNameImpl<ActionTextField>(utils::kEmailsFieldName);
    QString initialAdditionalRecipientsValue;

    QSignalBlocker targetUsersSignalBlocker{targetUsersField};
    QSignalBlocker additionalRecipientsSignalBlocker{additionalRecipientsField};

    Actions result;
    if (additionalRecipientsField && !additionalRecipientsField->value().isEmpty())
    {
        // It is required to take separate action for the additional recipients. The additional
        // recipients will be treated as power users, i.e., filtration is not required.

        NX_VERBOSE(
            this,
            "Building action for additional recipients: %1",
            additionalRecipientsField->value());

        targetUsersField->setSelection({}); //< Only additional recipients must be in the action.

        result.push_back(buildAction(aggregatedEvent));

        // All the rest actions must be created without additional recipients, to prevent action
        // duplication.
        initialAdditionalRecipientsValue = additionalRecipientsField->value();
        additionalRecipientsField->setValue({});
    }

    for (auto& [key, value]: eventUsersMap)
    {
        NX_VERBOSE(this, "Building action for users: %1", value.userIds);

        // Substitute the initial target users with the user the aggregated event has been filtered.
        targetUsersField->setSelection({
            .ids = std::move(value.userIds),
            .all = false});

        nx::i18n::ScopedLocalePtr scopedLocale;
        const auto locale = value.locale;
        if (locale.isEmpty())
        {
            NX_VERBOSE(
                this, "User language is not set, customization language will be used");
        }
        else
        {
            auto appContext = nx::vms::common::appContext();
            if (appContext) //< Application context is not initialized in unit tests.
            {
                // TODO: https://networkoptix.atlassian.net/browse/VMS-55348: This code leads to ~5
                // second delay in action generation during unit tests.  Added a flag in the test
                // to bypass this, but it should be fixed properly.
                if (!bypassScopedLocaleForTest)
                {
                    auto translationManager = appContext->translationManager();
                    if (translationManager) // Some unit tests do not have translations manager.
                        scopedLocale = translationManager->installScopedLocale(locale);
                }
            }
        }

        auto actions = filterActionPermissions(
            buildAction(value.event),
            targetUsersField->systemContext(),
            *actionManifest);

        result.insert(result.end(), actions.begin(), actions.end());
    }

    // Recover initial target users selection.
    targetUsersField->setSelection(initialTargetUsersValue);

    // Recover initial additional recipients value.
    if (additionalRecipientsField)
        additionalRecipientsField->setValue(initialAdditionalRecipientsValue);

    return result;
}

ActionPtr ActionBuilder::buildAction(const AggregatedEventPtr& aggregatedEvent)
{
    ActionPtr action(m_constructor());
    if (!action)
        return {};

    action->setRuleId(m_rule ? m_rule->id() : nx::Uuid());
    action->setOriginPeerId(engine()->router()->peerId());

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

    if (!isProlonged())
        action->setState(State::instant);

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
