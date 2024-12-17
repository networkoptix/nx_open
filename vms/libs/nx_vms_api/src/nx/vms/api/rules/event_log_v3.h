// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_types.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "../types/event_rule_types.h"

namespace nx::vms::api::rules {

/** Event log backwards compatibility structure. Shall be used in the /rest/v3/events/log only. */
struct NX_VMS_API EventParametersV3
{
    /**%apidoc[opt]
     * Type of the Event to be created. The default value is
     * <code>userDefinedEvent</code>
     */
    EventType eventType = EventType::undefinedEvent;

    /**%apidoc[opt]
     * Event date and time, as a string containing time in
     * milliseconds since epoch, or a local time formatted like
     * <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code> -
     * the format is auto-detected. If "timestamp" is absent, the current Server date and time
     * is used.
     */
    qint64 eventTimestampUsec = 0;

    /**%apidoc[opt] Event source - id of a Device, or a Server, or a PIR. */
    nx::Uuid eventResourceId;

    /**%apidoc[opt]
     * Name of the Device which has triggered the Event. It can be used
     * in a filter in Event Rules to assign different actions to different Devices. Also, the
     * user could see this name in the notification panel. Example: "POS terminal 5".
     */
    QString resourceName;

    /**%apidoc[opt] Id of a Server that generated the Event. */
    nx::Uuid sourceServerId;

    /**%apidoc[opt] Used for Reasoned Events as reason code. */
    EventReason reasonCode = EventReason::none;

    /**%apidoc[opt]
     * Used for Input Events only. Identifies the input port. Also used for the analytics event
     * type id.
     */
    QString inputPortId;

    /**%apidoc[opt]
     * Short Event description. It can be used in a filter in Event Rules to assign actions
     * depending on this text.
     */
    QString caption;

    /**%apidoc[opt]
     * Long Event description. It can be used as a filter in Event Rules to assign actions
     * depending on this text.
     */
    QString description;
};
#define EventParametersV3_Fields \
    (eventType)(eventTimestampUsec)(eventResourceId)(resourceName)(sourceServerId) \
    (reasonCode)(inputPortId)(caption)(description)
QN_FUSION_DECLARE_FUNCTIONS(EventParametersV3, (ubjson)(json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(EventParametersV3, EventParametersV3_Fields)

/** Event log backwards compatibility structure. Shall be used in the /rest/v3/events/log only. */
struct NX_VMS_API ActionParametersV3
{
    /**%apidoc Used for "Play Sound" / "Exec HTTP" Actions. */
    QString url;

    /**%apidoc */
    QString emailAddress;

    /**%apidoc Id of Device Output. */
    QString relayOutputId;

    /**%apidoc Text to be pronounced by the speech synthesizer. */
    QString sayText;

    /**%apidoc Bookmark. */
    QString tags;

    /**%apidoc
      * - For "Show Text Overlay" Action: generic text.
      * - For "Exec HTTP" Action: message body.
      * - For "Acknowledge" Action: bookmark description.
      * - For "CameraOutput" Action: additional JSON parameter for output action handler.
      */
    QString text;

    /**%apidoc
     * Generic additional Resource List.
     * - For "Show On Alarm Layout": contains the users.
     * - For "Acknowledge": contains the user that confirms the popup.
     */
    std::vector<nx::Uuid> additionalResources;

    /**%apidoc When set, signals that all users must be targeted by the Action. */
    bool allUsers = false;

    /**%apidoc For "Exec PTZ" - id of the preset Action. */
    QString presetId;

    /**%apidoc For "Alarm Layout" - if the source Resource should also be used. */
    bool useSource = false;

    /**%apidoc For "Exec HTTP" Action. */
    QString contentType;

    /**%apidoc HTTP method (empty string means auto-detection). */
    QString httpMethod;
};
#define ActionParametersV3_Fields (url)(emailAddress)(relayOutputId)(sayText)(tags)(text)\
    (additionalResources)(allUsers)(presetId)(useSource)(contentType)(httpMethod)

QN_FUSION_DECLARE_FUNCTIONS(ActionParametersV3, (ubjson)(json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(ActionParametersV3, ActionParametersV3_Fields)

/** Event log backwards compatibility structure. Shall be used in the /rest/v3/events/log only. */
struct NX_VMS_API EventLogRecordV3
{
    /**%apidoc Type of the Action. */
    ActionType actionType = ActionType::undefinedAction;

    /**%apidoc
     * Parameters of the Action. Only fields which are applicable to the particular Action are used.
     */
    ActionParametersV3 actionParams;

    /**%apidoc Parameters of the Event which triggered the Action. */
    EventParametersV3 eventParams;

    /**%apidoc Id of the Event Rule. */
    nx::Uuid businessRuleId;

    /**%apidoc Number of identical Events grouped into one. */
    int aggregationCount = 1;
};
#define EventLogRecordV3_Fields (actionType)(actionParams)(eventParams)(businessRuleId) \
    (aggregationCount)
QN_FUSION_DECLARE_FUNCTIONS(EventLogRecordV3, (ubjson)(json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(EventLogRecordV3, EventLogRecordV3_Fields)

using EventLogRecordV3List = std::vector<EventLogRecordV3>;

} // namespace nx::vms::api::rules
