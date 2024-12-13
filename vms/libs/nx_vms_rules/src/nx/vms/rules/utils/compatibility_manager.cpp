// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "compatibility_manager.h"

#include <nx/vms/common/system_context.h>

#include "../action_builder.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/sound_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/target_devices_field.h"
#include "../action_builder_fields/time_field.h"
#include "../actions/bookmark_action.h"
#include "../actions/show_notification_action.h"
#include "../engine.h"
#include "../event_filter.h"
#include "../event_filter_fields/source_camera_field.h"
#include "../event_filter_fields/state_field.h"
#include "../rule.h"
#include "common.h"
#include "event.h"
#include "field.h"
#include "type.h"

namespace nx::vms::rules::utils {

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

CompatibilityManager::CompatibilityManager(
    vms::rules::Rule* rule,
    common::SystemContext* context,
    QObject* parent)
    :
    QObject{parent},
    common::SystemContextAware{context},
    m_rule{rule},
    m_engine{context->vmsRulesEngine()},
    m_defaultActionType{kDefaultActionType}
{
    connect(
        m_rule->eventFilters().first(),
        &vms::rules::EventFilter::changed,
        this,
        &CompatibilityManager::onEventFilterFieldChanged);

    connect(
        m_rule->actionBuilders().first(),
        &vms::rules::ActionBuilder::changed,
        this,
        &CompatibilityManager::onActionBuilderFieldChanged);
}

void CompatibilityManager::changeEventType(const QString& eventType)
{
    if (m_rule->eventFilters().first()->eventType() == eventType)
        return;

    auto newEventFilter = m_engine->buildEventFilter(eventType);

    const auto eventDescriptor = m_engine->eventDescriptor(newEventFilter->eventType());
    const auto actionDescriptor =
        m_engine->actionDescriptor(m_rule->actionBuilders().first()->actionType());
    if (!NX_ASSERT(eventDescriptor && actionDescriptor))
        return;

    // Check the given event/action pair might be adjusted to a valid rule.
    if (!vms::rules::utils::isCompatible(eventDescriptor.value(), actionDescriptor.value()))
    {
        // When a user is changed event type and the new selected type is not compatible with the
        // current action type, action type must be set to some default compatible type.
        replaceActionBuilder(m_engine->buildActionBuilder(m_defaultActionType));
    }

    replaceEventFilter(std::move(newEventFilter));

    fixDurationValue();
    fixStateValue();
    fixUseSourceValue();

    checkCompatibility();

    emit ruleModified();
}

void CompatibilityManager::changeActionType(const QString& actionType)
{
    if (m_rule->actionBuilders().first()->actionType() == actionType)
        return;

    auto newActionBuilder = m_engine->buildActionBuilder(actionType);
    replaceActionBuilder(std::move(newActionBuilder));

    fixStateValue();
    fixUseSourceValue();

    checkCompatibility();

    emit ruleModified();
}

void CompatibilityManager::setDefaultActionType(const QString& type)
{
    m_defaultActionType = type;
}

void CompatibilityManager::onEventFilterFieldChanged(const QString& fieldName)
{
    if (fieldName == vms::rules::utils::kEventTypeIdFieldName)
        onAnalyticsEventTypeChanged();

    emit ruleModified();
}

void CompatibilityManager::onActionBuilderFieldChanged(const QString& fieldName)
{
    if (fieldName == vms::rules::utils::kDurationFieldName)
        onDurationChanged();

    emit ruleModified();
}

void CompatibilityManager::fixDurationValue()
{
    const auto durationField =
        m_rule->actionBuilders().first()->fieldByName<vms::rules::OptionalTimeField>(vms::rules::utils::kDurationFieldName);
    if (!durationField)
        return;

    const auto eventDurationType =
        vms::rules::getEventDurationType(systemContext()->vmsRulesEngine(), m_rule->eventFilters().first());
    if (eventDurationType == vms::rules::EventDurationType::instant
        && durationField->value() == std::chrono::microseconds::zero())
    {
        // Zero duration does not have sense for instant only event.
        const auto durationFieldProperties = durationField->properties();
        auto newValue = std::max(durationFieldProperties.value, durationFieldProperties.minimumValue);
        if (newValue == std::chrono::seconds::zero())
            newValue = std::chrono::seconds{1};

        QSignalBlocker blocker{durationField};
        durationField->setValue(newValue);
    }
}

void CompatibilityManager::fixStateValue()
{
    const auto eventFilter = m_rule->eventFilters().first();
    const auto stateField = eventFilter->fieldByType<vms::rules::StateField>();
    if (stateField)
    {
        const auto availableStates =
            vms::rules::utils::getPossibleFilterStates(systemContext()->vmsRulesEngine(), m_rule);
        if (!NX_ASSERT(!availableStates.empty()))
            return;

        if (!availableStates.contains(stateField->value()))
        {
            // If the current state is not valid use any from the available.
            QSignalBlocker blocker{stateField};
            stateField->setValue(availableStates.first());
        }
    }
}

void CompatibilityManager::fixUseSourceValue()
{
    const auto eventFilter = m_rule->eventFilters().first();
    const auto actionBuilder = m_rule->actionBuilders().first();

    const auto targetDeviceField = actionBuilder->fieldByName<vms::rules::TargetDevicesField>(
        vms::rules::utils::kDeviceIdsFieldName);
    const auto targetSingleDeviceField =
        actionBuilder->fieldByName<vms::rules::TargetDeviceField>(
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

void CompatibilityManager::onDurationChanged()
{
    // During the rule editing by the user, some values might becomes inconsistent. The code below
    // is fixing such a values.

    const auto actionBuilder = m_rule->actionBuilders().first();

    const auto durationField = actionBuilder->fieldByName<vms::rules::OptionalTimeField>(
        vms::rules::utils::kDurationFieldName);
    if (!NX_ASSERT(durationField))
        return;

    fixDurationValue();
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

    checkCompatibility();
}

void CompatibilityManager::onAnalyticsEventTypeChanged()
{
    // When analytics type is changed event duration might be changed. It is required to check
    // rule compatibility here.

    const auto eventDescriptor = m_engine->eventDescriptor(
        m_rule->eventFilters().first()->eventType());
    const auto actionDescriptor =
        m_engine->actionDescriptor(m_rule->actionBuilders().first()->actionType());
    if (!NX_ASSERT(eventDescriptor && actionDescriptor))
        return;

    if (!vms::rules::utils::isCompatible(eventDescriptor.value(), actionDescriptor.value()))
    {
        auto newActionBuilder =
            m_engine->buildActionBuilder(kDefaultActionType);

        m_rule->takeActionBuilder(0);
        m_rule->addActionBuilder(std::move(newActionBuilder));

        connect(
            m_rule->actionBuilders().first(),
            &vms::rules::ActionBuilder::changed,
            this,
            &CompatibilityManager::onActionBuilderFieldChanged);
    }

    fixDurationValue();
    fixStateValue();
    fixUseSourceValue();

    checkCompatibility();
}

void CompatibilityManager::replaceEventFilter(
    std::unique_ptr<vms::rules::EventFilter>&& eventFilter)
{
    auto oldEventFilter = m_rule->takeEventFilter(0);
    if (auto oldSourceCameraField = oldEventFilter->fieldByType<vms::rules::SourceCameraField>())
        m_lastEventCamerasSelection = oldSourceCameraField->ids();

    auto newSourceCameraField = eventFilter->fieldByType<vms::rules::SourceCameraField>();
    if (newSourceCameraField && m_lastEventCamerasSelection)
    {
        newSourceCameraField->setIds(m_lastEventCamerasSelection.value());
        if (!m_lastEventCamerasSelection.value().isEmpty())
            newSourceCameraField->setAcceptAll(false);
    }

    m_rule->addEventFilter(std::move(eventFilter));

    connect(
        m_rule->eventFilters().first(),
        &vms::rules::EventFilter::changed,
        this,
        &CompatibilityManager::onEventFilterFieldChanged);
}

void CompatibilityManager::replaceActionBuilder(
    std::unique_ptr<vms::rules::ActionBuilder>&& actionBuilder)
{
    auto oldActionBuilder = m_rule->takeActionBuilder(0);

    if (auto targetDeviceField = oldActionBuilder->fieldByType<vms::rules::TargetDeviceField>())
        m_lastActionCamerasSelection = {targetDeviceField->id()};

    if (auto targetDevicesField = oldActionBuilder->fieldByType<vms::rules::TargetDevicesField>())
        m_lastActionCamerasSelection = targetDevicesField->ids();

    if (auto soundField = oldActionBuilder->fieldByType<vms::rules::SoundField>())
        m_lastSoundSelection = soundField->value();

    if (m_lastActionCamerasSelection)
    {
        if (auto targetDeviceField = actionBuilder->fieldByType<vms::rules::TargetDeviceField>())
        {
            targetDeviceField->setId(m_lastActionCamerasSelection->empty()
                ? nx::Uuid{}
                : *m_lastActionCamerasSelection->begin());
        }
        else if (auto targetDevicesField = actionBuilder->fieldByType<vms::rules::TargetDevicesField>())
        {
            targetDevicesField->setIds(m_lastActionCamerasSelection.value());
        }
    }

    if (m_lastSoundSelection)
    {
        if (auto newSoundField = actionBuilder->fieldByType<vms::rules::SoundField>())
            newSoundField->setValue(m_lastSoundSelection.value());
    }

    m_rule->addActionBuilder(std::move(actionBuilder));

    connect(
        m_rule->actionBuilders().first(),
        &vms::rules::ActionBuilder::changed,
        this,
        &CompatibilityManager::onActionBuilderFieldChanged);
}

void CompatibilityManager::checkCompatibility()
{
    // Rule with the adjusted parameters must be valid.
    NX_ASSERT(
        vms::rules::utils::isCompatible(
            systemContext()->vmsRulesEngine(),
            m_rule->eventFilters().first(),
            m_rule->actionBuilders().first()),
        makeCompatibilityAlertMessage(m_rule));
}

} // namespace nx::vms::rules::utils
