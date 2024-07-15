// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rule_compatibility_manager.h"

#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_single_device_field.h>
#include <nx/vms/rules/action_builder_fields/time_field.h>
#include <nx/vms/rules/actions/bookmark_action.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/common.h>
#include <nx/vms/rules/utils/event.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::client::desktop::rules {

namespace {

// Notification action is compatible with any event type.
const auto kDefaultActionType = vms::rules::utils::type<vms::rules::NotificationAction>();

QString makeCompatibilityAlertMessage(const vms::rules::Rule* rule)
{
    return QString{"Rule compatibility is broken for the `%1` event and `%2` action pair"}
        .arg(rule->eventFilters().first()->eventType())
        .arg(rule->actionBuilders().first()->actionType());
}

} // namespace

RuleCompatibilityManager::RuleCompatibilityManager(
    vms::rules::Rule* rule,
    SystemContext* context,
    QObject* parent)
    :
    QObject{parent},
    SystemContextAware{context},
    m_rule{rule},
    m_engine{context->vmsRulesEngine()}
{
    connect(
        m_rule->eventFilters().first(),
        &vms::rules::EventFilter::changed,
        this,
        &RuleCompatibilityManager::onEventFilterFieldChanged);

    connect(
        m_rule->actionBuilders().first(),
        &vms::rules::ActionBuilder::changed,
        this,
        &RuleCompatibilityManager::onActionBuilderFieldChanged);
}

bool RuleCompatibilityManager::changeEventType(const QString& eventType)
{
    auto newEventFilter = m_engine->buildEventFilter(eventType);

    if (!vms::rules::utils::isCompatible(
        systemContext()->vmsRulesEngine(),
        newEventFilter.get(),
        m_rule->actionBuilders().first()))
    {
        // When a user is changed event type and the new selected type is not compatible with the
        // current action type, action type must be set to some default compatible type.
        replaceActionBuilder(m_engine->buildActionBuilder(kDefaultActionType));
    }

    replaceEventFilter(std::move(newEventFilter));

    fixStateValue();
    fixUseSourceValue();

    return NX_ASSERT(
        vms::rules::utils::isCompatible(
            systemContext()->vmsRulesEngine(),
            m_rule->eventFilters().first(),
            m_rule->actionBuilders().first()),
        makeCompatibilityAlertMessage(m_rule));
}

bool RuleCompatibilityManager::changeActionType(const QString& actionType)
{
    auto newActionBuilder = m_engine->buildActionBuilder(actionType);

    if (!vms::rules::utils::isCompatible(
        systemContext()->vmsRulesEngine(),
        m_rule->eventFilters().first(),
        newActionBuilder.get()))
    {
        // User must not have an ability to select invalid action type.
        return false;
    }

    replaceActionBuilder(std::move(newActionBuilder));

    fixStateValue();
    fixUseSourceValue();

    return NX_ASSERT(
        vms::rules::utils::isCompatible(
            systemContext()->vmsRulesEngine(),
            m_rule->eventFilters().first(),
            m_rule->actionBuilders().first()),
        makeCompatibilityAlertMessage(m_rule));
}

void RuleCompatibilityManager::onEventFilterFieldChanged(const QString& fieldName)
{
    if (fieldName == vms::rules::utils::kEventTypeIdFieldName)
        onAnalyticsEventTypeChanged();
}

void RuleCompatibilityManager::onActionBuilderFieldChanged(const QString& fieldName)
{
    if (fieldName == vms::rules::utils::kDurationFieldName)
        onDurationChanged();
}

void RuleCompatibilityManager::fixStateValue()
{
    const auto eventFilter = m_rule->eventFilters().first();
    const auto stateField = eventFilter->fieldByType<vms::rules::StateField>();
    if (stateField)
    {
        const auto availableStates =
            vms::rules::utils::getAvailableStates(systemContext()->vmsRulesEngine(), m_rule);
        if (!NX_ASSERT(!availableStates.empty()))
            return;

        if (!availableStates.contains(stateField->value()))
        {
            // If the current state is not valid use any from the available.
            QSignalBlocker blocker{stateField};
            stateField->setValue(*availableStates.begin());
        }
    }
}

void RuleCompatibilityManager::fixUseSourceValue()
{
    const auto eventFilter = m_rule->eventFilters().first();
    const auto actionBuilder = m_rule->actionBuilders().first();

    const auto targetDeviceField = actionBuilder->fieldByName<vms::rules::TargetDeviceField>(
        vms::rules::utils::kDeviceIdsFieldName);
    const auto targetSingleDeviceField =
        actionBuilder->fieldByName<vms::rules::TargetSingleDeviceField>(
            vms::rules::utils::kCameraIdFieldName);

    if (!targetDeviceField && !targetSingleDeviceField)
        return;

    auto eventDescriptor =
        systemContext()->vmsRulesEngine()->eventDescriptor(eventFilter->eventType());
    const auto hasSourceCamera = vms::rules::hasSourceCamera(eventDescriptor.value());

    if (targetDeviceField && targetDeviceField->useSource() && !hasSourceCamera)
    {
        QSignalBlocker blocker{targetDeviceField};
        targetDeviceField->setUseSource(false);
    }

    if (targetSingleDeviceField && targetSingleDeviceField->useSource() && !hasSourceCamera)
    {
        QSignalBlocker blocker{targetSingleDeviceField};
        targetSingleDeviceField->setUseSource(false);
    }
}

void RuleCompatibilityManager::onDurationChanged()
{
    // During the rule editing by the user, some values might becomes inconsistent. The code below
    // is fixing such a values.

    const auto actionBuilder = m_rule->actionBuilders().first();
    const auto eventFilter = m_rule->eventFilters().first();

    const auto durationField = actionBuilder->fieldByName<vms::rules::OptionalTimeField>(
        vms::rules::utils::kDurationFieldName);
    if (!NX_ASSERT(durationField))
        return;

    const auto eventDurationType =
        vms::rules::getEventDurationType(systemContext()->vmsRulesEngine(), eventFilter);
    if (eventDurationType == vms::rules::EventDurationType::instant
        && durationField->value() == std::chrono::microseconds::zero())
    {
        // Zero duration does not have sense for instant only event.
        const auto defaultDuration = durationField->properties().defaultValue;

        QSignalBlocker blocker{durationField};
        if (NX_ASSERT(
            defaultDuration != std::chrono::seconds::zero(),
            "Default value for the '%1' field in the '%2' action cannot be zero, fix the manifest",
            vms::rules::utils::kDurationFieldName,
            actionBuilder->actionType()))
        {
            durationField->setValue(defaultDuration);
        }
        else
        {
            durationField->setValue(std::chrono::seconds{1});
        }
    }

    fixStateValue();

    // When action has non zero duration, pre and post recording (except bookmark action
    // pre-recording) fields must have zero value.
    const auto fixTimeField =
        [actionBuilder, durationField](const QString& fieldName)
        {
            const auto timeField = actionBuilder->fieldByName<vms::rules::TimeField>(fieldName);
            if (!timeField)
                return;

            if (durationField && durationField->value() != std::chrono::microseconds::zero())
            {
                QSignalBlocker blocker{timeField};
                timeField->setValue(std::chrono::microseconds::zero());
            }
        };

    if (vms::rules::utils::type<vms::rules::BookmarkAction>() != actionBuilder->actionType())
        fixTimeField(vms::rules::utils::kRecordBeforeFieldName);

    fixTimeField(vms::rules::utils::kRecordAfterFieldName);
}

void RuleCompatibilityManager::onAnalyticsEventTypeChanged()
{
    // When analytics type is changed event duration might be changed. It is required to check
    // rule compatibility here.

    if (!vms::rules::utils::isCompatible(
        systemContext()->vmsRulesEngine(),
        m_rule->eventFilters().first(),
        m_rule->actionBuilders().first()))
    {
        auto newActionBuilder =
            m_engine->buildActionBuilder(kDefaultActionType);

        m_rule->takeActionBuilder(0);
        m_rule->addActionBuilder(std::move(newActionBuilder));

        connect(
            m_rule->actionBuilders().first(),
            &vms::rules::ActionBuilder::changed,
            this,
            &RuleCompatibilityManager::onActionBuilderFieldChanged);
    }

    fixStateValue();
}

void RuleCompatibilityManager::replaceEventFilter(
    std::unique_ptr<vms::rules::EventFilter>&& eventFilter)
{
    m_rule->takeEventFilter(0);
    m_rule->addEventFilter(std::move(eventFilter));

    connect(
        m_rule->eventFilters().first(),
        &vms::rules::EventFilter::changed,
        this,
        &RuleCompatibilityManager::onEventFilterFieldChanged);
}

void RuleCompatibilityManager::replaceActionBuilder(
    std::unique_ptr<vms::rules::ActionBuilder>&& actionBuilder)
{
    m_rule->takeActionBuilder(0);
    m_rule->addActionBuilder(std::move(actionBuilder));

    connect(
        m_rule->actionBuilders().first(),
        &vms::rules::ActionBuilder::changed,
        this,
        &RuleCompatibilityManager::onActionBuilderFieldChanged);
}

} // namespace nx::vms::client::desktop::rules
