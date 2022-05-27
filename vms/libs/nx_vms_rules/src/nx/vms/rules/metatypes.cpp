// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metatypes.h"

#include <analytics/common/object_metadata.h>
#include <common/common_meta_types.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/rules/camera_conflict_list.h>

#include "basic_event.h"
#include "field_types.h"

namespace nx::vms::rules {

// TODO: Consider moving this registration to the API library.
void Metatypes::initialize()
{
    static std::atomic_bool initialized = false;

    if (initialized.exchange(true))
        return;

    QnCommonMetaTypes::initialize();

    // Register types and serializers in alphabetical order.

    QnJsonSerializer::registerSerializer<nx::common::metadata::Attributes>();

    qRegisterMetaType<nx::vms::api::EventLevel>();
    QnJsonSerializer::registerSerializer<nx::vms::api::EventLevel>();

    qRegisterMetaType<nx::vms::api::EventLevels>();
    QnJsonSerializer::registerSerializer<nx::vms::api::EventLevels>();

    QnJsonSerializer::registerSerializer<nx::vms::api::EventReason>();

    qRegisterMetaType<nx::vms::api::rules::State>();
    QnJsonSerializer::registerSerializer<State>();

    qRegisterMetaType<nx::vms::rules::CameraConflictList>();
    QnJsonSerializer::registerSerializer<nx::vms::rules::CameraConflictList>();

    qRegisterMetaType<nx::vms::rules::EventPtr>();

    qRegisterMetaType<QnUuidSet>("QnUuidSet");
    QnJsonSerializer::registerSerializer<QnUuidSet>();

    QnJsonSerializer::registerSerializer<std::chrono::microseconds>();
    QnJsonSerializer::registerSerializer<std::chrono::seconds>();
};

} // namespace nx::vms::rules
