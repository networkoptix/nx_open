// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action.h"

#include <core/resource/user_resource.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

#include "../action_builder.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../basic_action.h"
#include "../engine.h"
#include "../manifest.h"
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

bool checkUserPermissions(const QnUserResourcePtr& user, const ActionPtr& action)
{
    if (!user)
        return false;

    const auto propValue = action->property(utils::kUsersFieldName);
    if (!propValue.isValid() || !propValue.canConvert<UuidSelection>())
        return false;

    const auto userSelection = propValue.value<UuidSelection>();
    if (userSelection.all || userSelection.ids.contains(user->getId()))
        return true;

    const auto userGroups = nx::vms::common::userGroupsWithParents(user);
    return userGroups.intersects(userSelection.ids);
}

} // namespace nx::vms::rules
