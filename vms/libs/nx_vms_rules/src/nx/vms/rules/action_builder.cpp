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
#include <nx/utils/log/assert.h>
#include <nx/utils/qobject.h>
#include <nx/vms/common/system_context.h>

#include "action_field.h"
#include "action_fields/target_user_field.h"
#include "aggregated_event.h"
#include "aggregator.h"
#include "basic_action.h"
#include "basic_event.h"
#include "utils/field.h"
#include "utils/type.h"

namespace nx::vms::rules {

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

void ActionBuilder::addField(const QString& name, std::unique_ptr<ActionField> field)
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

const QHash<QString, ActionField*> ActionBuilder::fields() const
{
    QHash<QString, ActionField*> result;
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

void ActionBuilder::process(EventPtr event)
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

    if (m_fields.contains(utils::kUsersFieldName))
        buildAndEmitActionForTargetUsers(aggregatedEvent);
    else
        emit action(buildAction(aggregatedEvent));
}

void ActionBuilder::buildAndEmitActionForTargetUsers(const AggregatedEventPtr& aggregatedEvent)
{
    auto targetUsersField = fieldByNameImpl<TargetUserField>(utils::kUsersFieldName);
    if (!NX_ASSERT(targetUsersField))
        return;

    UuidSelection initialFieldValue {
        .ids = targetUsersField->ids(),
        .all = targetUsersField->acceptAll()
    };
    QSignalBlocker signalBlocker{targetUsersField};

    // If the action should be shown to some users it is required to check if the user has
    // appropriate rights to see the event details.
    for (const auto& user: targetUsersField->users()) //< TODO: Unite users with the same access rights.
    {
        // Checks whether the user has rights to view the event. At the moment only cameras are
        // required to be checked.
        const auto filter =
            [&user, context = targetUsersField->systemContext()](const EventPtr& event)
            {
                auto cameraIdProperty = event->property(utils::kCameraIdFieldName);
                if (!cameraIdProperty.isValid() || !cameraIdProperty.canConvert<QnUuid>())
                    return true;

                QnUuid cameraId = cameraIdProperty.value<QnUuid>();

                const auto cameraResource =
                    context->resourcePool()->getResourceById<QnVirtualCameraResource>(cameraId);
                if (!cameraResource)
                    return true;

                return context->resourceAccessManager()->hasPermission(
                    user,
                    cameraResource,
                    Qn::Permission::ViewContentPermission);
            };

        auto filteredAggregatedEvent = aggregatedEvent->filtered(filter);

        if (!filteredAggregatedEvent)
            continue;

        // Substitute the initial target users with the user the aggregated event has been filtered.
        targetUsersField->setSelection({
            .ids = {user->getId()},
            .all = false});

        NX_VERBOSE(this, "Building action for user: %1", user->getName());
        emit action(buildAction(filteredAggregatedEvent));
    }

    // Recover initial target users selection.
    targetUsersField->setSelection(initialFieldValue);
}

ActionPtr ActionBuilder::buildAction(const AggregatedEventPtr& aggregatedEvent)
{
    ActionPtr action(m_constructor());
    if (!action)
        return {};

    action->setRuleId(m_ruleId);

    const auto propertyNames =
        nx::utils::propertyNames(action.get(), nx::utils::PropertyAccess::writable);
    for (const auto& propertyName: propertyNames)
    {
        const auto propertyNameUtf8 = propertyName.toUtf8();

        if (m_fields.contains(propertyName))
        {
            auto& field = m_fields.at(propertyName);
            const auto value = field->build(aggregatedEvent);
            action->setProperty(propertyNameUtf8, value);
        }
        else
        {
            // Set property value only if it exists.
            if (action->property(propertyNameUtf8).isValid()
                && aggregatedEvent->property(propertyNameUtf8).isValid())
            {
                action->setProperty(propertyNameUtf8, aggregatedEvent->property(propertyNameUtf8));
            }
        }
    }

    return action;
}

void ActionBuilder::handleAggregatedEvents()
{
    for (const auto& aggregationInfo: m_aggregator->popEvents())
        buildAndEmitAction(AggregatedEventPtr::create(aggregationInfo));
}

} // namespace nx::vms::rules
