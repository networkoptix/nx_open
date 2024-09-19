// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_action.h"

#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/target_devices_field.h"
#include "../action_builder_fields/text_field.h"
#include "../action_builder_fields/time_field.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

const ItemDescriptor& BookmarkAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<BookmarkAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Create Bookmark")),
        .description = "Create Bookmark",
        .flags = ItemFlag::prolonged,
        .executionTargets = ExecutionTarget::servers,
        .targetServers = TargetServers::resourceOwner,
        .fields = {
            makeFieldDescriptor<TargetDevicesField>(
                utils::kDeviceIdsFieldName,
                Strings::at(),
                {},
                ResourceFilterFieldProperties{
                    .base = FieldProperties{.optional = false},
                    .validationPolicy = kBookmarkValidationPolicy
                }.toVariantMap()),
            utils::makeDurationFieldDescriptor(
                TimeFieldProperties{
                    .value = 5s,
                    .maximumValue = 600s,
                    .minimumValue = 5s}.toVariantMap(),
                    {},
                "Minimum duration of the bookmark."),
            utils::makeTimeFieldDescriptor<TimeField>(
                vms::rules::utils::kRecordBeforeFieldName,
                Strings::preRecording(),
                "The amount of time to add to the bookmark duration before the event's start time.",
                TimeFieldProperties{
                    .value = 1s,
                    .maximumValue = 600s,
                    .minimumValue = 0s}.toVariantMap()),
            utils::makeTimeFieldDescriptor<TimeField>(
                vms::rules::utils::kRecordAfterFieldName,
                Strings::postRecording(),
                "The amount of time to add to the bookmark duration after the event's start time.",
                TimeFieldProperties{
                    .value = 0s,
                    .maximumValue = 600s,
                    .minimumValue = 0s}.toVariantMap()),
            makeFieldDescriptor<ActionTextField>(
                utils::kTagsFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Add Tags")),
                "List of tags associated with the bookmark. "
                    "To assign multiple tags, separate them with commas."),

            // TODO: #amalov Use Qn::ResouceInfoLevel::RI_WithUrl & AttrSerializePolicy::singleLine
            utils::makeExtractDetailFieldDescriptor(
                utils::kNameFieldName,
                utils::kExtendedCaptionDetailName),
            utils::makeExtractDetailFieldDescriptor(
                utils::kDescriptionFieldName,
                utils::kDetailingDetailName),
        },
        .resources = {{utils::kDeviceIdsFieldName, {ResourceType::device, {}, {}, FieldFlag::target}}},
    };
    return kDescriptor;
}

QVariantMap BookmarkAction::details(common::SystemContext* context) const
{
    auto result = base_type::details(context);

    utils::insertIfNotEmpty(result, utils::kDescriptionDetailName, description());

    return result;
}

} // namespace nx::vms::rules
