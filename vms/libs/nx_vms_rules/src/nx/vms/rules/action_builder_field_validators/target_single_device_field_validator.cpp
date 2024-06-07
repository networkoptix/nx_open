// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_single_device_field_validator.h"

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <nx/vms/rules/camera_validation_policy.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/field.h>

#include "../action_builder_fields/target_single_device_field.h"
#include "../strings.h"
#include "../utils/validity.h"

namespace nx::vms::rules {

ValidationResult TargetSingleDeviceFieldValidator::validity(
    const Field* field, const Rule* rule, common::SystemContext* context) const
{
    auto targetSingleDeviceField = dynamic_cast<const TargetSingleDeviceField*>(field);
    if (!NX_ASSERT(targetSingleDeviceField))
        return {QValidator::State::Invalid, {Strings::invalidFieldType()}};

    const auto deviceId = targetSingleDeviceField->id();
    const auto targetSingleDeviceFieldProperties = targetSingleDeviceField->properties();
    const bool isValidSelection = !deviceId.isNull()
        || targetSingleDeviceField->useSource()
        || targetSingleDeviceFieldProperties.allowEmptySelection;

    if (!isValidSelection)
    {
        return {
            QValidator::State::Invalid,
            Strings::selectCamera(context, /*allowMultipleSelection*/ false)};
    }

    if (!targetSingleDeviceFieldProperties.validationPolicy.isEmpty())
    {
        const auto device =
            context->resourcePool()->getResourceById<QnVirtualCameraResource>(deviceId);

        if (targetSingleDeviceFieldProperties.validationPolicy == kExecPtzValidationPolicy)
            return utils::cameraValidity<QnExecPtzPresetPolicy>(context, device);

        if (targetSingleDeviceFieldProperties.validationPolicy == kCameraFullScreenValidationPolicy)
        {
            if (targetSingleDeviceField->useSource())
                return {};

            auto targetLayoutField = rule->actionBuilders().front()->fieldByName<TargetLayoutField>(
                utils::kLayoutIdsFieldName);
            if (!NX_ASSERT(targetLayoutField))
            {
                return {
                    QValidator::State::Invalid,
                    Strings::fieldValueMustBeProvided(utils::kLayoutIdsFieldName)};
            }

            const auto layouts =
                context->resourcePool()->getResourcesByIds<QnLayoutResource>(targetLayoutField->value());
            if (layouts.empty())
                return {};

            for (const auto& layout: layouts)
            {
                const auto layoutItem = layout->getItems();
                bool isCameraOnLayout = std::any_of(
                    layoutItem.cbegin(),
                    layoutItem.cend(),
                    [&device, &context](const common::LayoutItemData& item)
                    {
                        return context->resourcePool()->getResourceByDescriptor(item.resource) == device;
                    });

                if (!isCameraOnLayout)
                {
                    auto alert = layouts.size() == 1
                        ? tr("This camera is not currently on the selected layout")
                        : tr("This camera is not currently on some of the selected layouts");

                    return {QValidator::State::Intermediate, alert};
                }
            }

            return utils::cameraValidity<QnFullscreenCameraPolicy>(context, device);
        }

        return {QValidator::State::Invalid, Strings::unexpectedPolicy()};
    }

    return {};
}

} // namespace nx::vms::rules
