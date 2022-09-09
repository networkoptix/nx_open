// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/action_parameters.h>

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules {

using ActionType = nx::vms::api::ActionType;
using EventParameters = nx::vms::event::EventParameters;
using ActionParameters = nx::vms::event::ActionParameters;

struct SubstitutionKeywords
{
    struct NX_VMS_COMMON_API Event
    {
        static const QString source;
        static const QString caption;
        static const QString description;
        static const QString cameraId;
        static const QString cameraName;
        static const QString eventType;
        static const QString eventName;
    };
};

/**
 * @return Actual action execution parameters depending on the actual input data. For example,
 *     if action parameters text field contains `{description}` keyword, it will be replaced with
 *     the event parameters description field contents.
 * @param actionType Type of the action.
 * @param sourceActionParameters Parameters that were used to setup the action in the rule.
 * @param eventRuntimeParameters Actual parameters event was triggered with.
 * @param systemContext Context of the System.
 */
NX_VMS_COMMON_API ActionParameters actualActionParameters(
    ActionType actionType,
    const ActionParameters& sourceActionParameters,
    const EventParameters& eventRuntimeParameters,
    const nx::vms::common::SystemContext* systemContext);

} // namespace nx::vms::rules
