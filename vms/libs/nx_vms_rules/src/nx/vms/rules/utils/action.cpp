// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action.h"

#include <core/resource/user_resource.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

#include "../action_builder.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../basic_action.h"
#include "../engine.h"
#include "../manifest.h"
#include "common.h"
#include "field.h"

namespace nx::vms::rules {

bool isProlonged(const Engine* engine, const ActionBuilder* builder)
{
    const auto manifest = engine->actionDescriptor(builder->actionType());
    if (!manifest || !manifest->flags.testFlag(ItemFlag::prolonged))
        return false;

    const auto durationField = builder->fieldByName<OptionalTimeField>(utils::kDurationFieldName);
    return (!durationField || durationField->value() <= std::chrono::seconds::zero());
}

bool hasDuration(const vms::rules::ItemDescriptor& actionDescriptor)
{
    return utils::fieldByName(utils::kDurationFieldName, actionDescriptor).has_value();
}

bool hasTargetCamera(const vms::rules::ItemDescriptor& actionDescriptor)
{
    return std::any_of(
        actionDescriptor.fields.begin(),
        actionDescriptor.fields.end(),
        [](const vms::rules::FieldDescriptor& fieldDescriptor)
        {
            return fieldDescriptor.fieldName == vms::rules::utils::kDeviceIdsFieldName
                || fieldDescriptor.fieldName == vms::rules::utils::kCameraIdFieldName;
        });
}

bool hasTargetLayout(const vms::rules::ItemDescriptor& actionDescriptor)
{
    return std::any_of(
        actionDescriptor.fields.begin(),
        actionDescriptor.fields.end(),
        [](const vms::rules::FieldDescriptor& fieldDescriptor)
        {
            return fieldDescriptor.fieldName == vms::rules::utils::kLayoutIdsFieldName;
        });
}

bool hasTargetUser(const vms::rules::ItemDescriptor& actionDescriptor)
{
    return std::any_of(
        actionDescriptor.fields.begin(),
        actionDescriptor.fields.end(),
        [](const vms::rules::FieldDescriptor& fieldDescriptor)
        {
            return fieldDescriptor.fieldName == vms::rules::utils::kUsersFieldName;
        });
}

bool hasTargetServer(const vms::rules::ItemDescriptor& actionDescriptor)
{
    return std::any_of(
        actionDescriptor.fields.begin(),
        actionDescriptor.fields.end(),
        [](const vms::rules::FieldDescriptor& fieldDescriptor)
        {
            return fieldDescriptor.fieldName == vms::rules::utils::kServerIdsFieldName;
        });
}

bool needAcknowledge(const ActionPtr& action)
{
    return action->property(utils::kAcknowledgeFieldName).toBool();
}

QSet<State> getPossibleFilterStatesForActionDescriptor(
    const vms::rules::ItemDescriptor& actionDescriptor)
{
    if (actionDescriptor.flags.testFlag(ItemFlag::instant))
        return {State::instant, State::started, State::stopped};

    return utils::fieldByName(utils::kDurationFieldName, actionDescriptor)
        ? QSet<State>{State::instant, State::started, State::stopped}
        : QSet<State>{State::none};
}

QSet<State> getPossibleFilterStatesForActionBuilder(
    const Engine* engine, const ActionBuilder* actionBuilder)
{
    if (isProlonged(engine, actionBuilder))
        return {State::none};

    return {State::instant, State::started, State::stopped};
}

} // namespace nx::vms::rules
