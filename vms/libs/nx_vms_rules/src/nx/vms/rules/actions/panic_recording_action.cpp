// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "panic_recording_action.h"

#include "../manifest.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& PanicRecordingAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PanicRecordingAction>(),
        .displayName = tr("Panic recording"),
        .description = tr("Panic Recording mode switches recording settings for all cameras"
            " to maximum FPS and quality."),
        .flags = ItemFlag::prolonged,
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
