// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "panic_recording_action.h"

#include "../action_builder_fields/optional_time_field.h"
#include "../manifest.h"
#include "../utils/field.h"
#include "../utils/type.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

const ItemDescriptor& PanicRecordingAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PanicRecordingAction>(),
        .displayName = tr("Panic Recording"),
        .description = tr("Panic Recording mode switches recording settings for all cameras"
            " to maximum FPS and quality."),
        .flags = ItemFlag::prolonged,
        .fields = {
            utils::makeTimeFieldDescriptor<OptionalTimeField>(
                    utils::kDurationFieldName,
                    tr("Duration"),
                    {},
                    {.initialValue = 5s, .defaultValue = 5s, .maximumValue = 9999h, .minimumValue = 1s}),
            utils::makeIntervalFieldDescriptor(tr("Interval of Action"))
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
