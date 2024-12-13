// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_overlay_action.h"

#include <nx/vms/common/html/html.h>

#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/target_devices_field.h"
#include "../action_builder_fields/target_users_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

const ItemDescriptor& TextOverlayAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<TextOverlayAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Show Text Overlay")),
        .description = "Displays a text overlay on layout items for specified devices.",
        .flags = ItemFlag::prolonged,
        .executionTargets = ExecutionTarget::clients,
        .fields = {
            makeFieldDescriptor<TargetDevicesField>(utils::kDeviceIdsFieldName,
                Strings::at(),
                {},
                ResourceFilterFieldProperties{.base = FieldProperties{.optional = false}}
                    .toVariantMap()),
            utils::makeDurationFieldDescriptor(
                TimeFieldProperties{
                    .value = 5s,
                    .maximumValue = 30s,
                    .minimumValue = 5s}.toVariantMap(),
                    Strings::for_()),
            makeFieldDescriptor<TextWithFields>(
                utils::kTextFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Custom Text")),
                "Text to display."),
            makeFieldDescriptor<TargetUsersField>(
                utils::kUsersFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Show To")),
                /*description*/ {},
                ResourceFilterFieldProperties{
                    .base = FieldProperties{.visible = false},
                    .acceptAll = true
                }.toVariantMap()),
            // TODO: #amalov Use Qn::ResouceInfoLevel::RI_WithUrl & AttrSerializePolicy::singleLine
            utils::makeExtractDetailFieldDescriptor(
                utils::kExtendedCaptionFieldName,
                utils::kExtendedCaptionDetailName),
            utils::makeExtractDetailFieldDescriptor(
                utils::kDetailingFieldName,
                utils::kDetailingDetailName),
        },
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device, Qn::ViewContentPermission}},
            {utils::kUsersFieldName, {ResourceType::user}},
        },
    };
    return kDescriptor;
}

QVariantMap TextOverlayAction::details(common::SystemContext* context) const
{
    auto result = base_type::details(context);

    auto text = this->text().trimmed();
    if (!text.isEmpty())
        text = nx::vms::common::html::toPlainText(text);

    utils::insertIfNotEmpty(result, utils::kDescriptionDetailName, text);

    return result;
}

} // namespace nx::vms::rules
