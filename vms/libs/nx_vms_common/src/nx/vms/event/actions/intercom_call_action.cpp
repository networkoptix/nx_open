// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "intercom_call_action.h"

#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/event/action_parameters.h>

namespace nx::vms::event {

IntercomCallAction::IntercomCallAction(
    common::system_health::MessageType message,
    nx::Uuid eventResourceId,
    const nx::common::metadata::Attributes& attributes)
    :
    base_type(message, eventResourceId, attributes)
{
    NX_ASSERT(message == common::system_health::MessageType::showIntercomInformer
        || message == common::system_health::MessageType::showMissedCallInformer
        || message == common::system_health::MessageType::rejectIntercomCall);

    getParams().additionalResources.push_back(api::kAdvancedViewersGroupId);
}

} // namespace nx::vms::event
