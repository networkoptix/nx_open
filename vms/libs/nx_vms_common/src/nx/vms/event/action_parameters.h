// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_types.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace vms {
namespace event {

struct NX_VMS_COMMON_API ActionParameters
{
    /**%apidoc
      * Identifier of the Action - used in "Show/Hide Popup" Action.
      */
    QnUuid actionId;

    /**%apidoc
      * Shows if the Action should be confirmed by the user. Currently used for Bookmarks confirmation.
      */
    bool needConfirmation = false;

    /**%apidoc
     *  Additional parameter for Event Log convenience. Is not filled when the Action actually occurs.
     */
    QnUuid actionResourceId;

    /**%apidoc Used for "Play Sound" / "Exec HTTP" Actions. */
    QString url;

    /**%apidoc */
    QString emailAddress;

    /**%apidoc Frames per second for recording. */
    int fps = 10;

    /**%apidoc Stream quality for recording. */
    Qn::StreamQuality streamQuality = Qn::StreamQuality::highest;

    /**%apidoc
     * Field is used in different scenarios, the unit depends on action type:
     * - cameraRecordingAction - time the recording continues after the event end, in seconds.
     * - bookmarkAction - the epsilon which the end time is extended by, in milliseconds.
     */
    int recordAfter = 0;

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

    static constexpr int kDefaultFixedDurationMs = 5000;

    /**%apidoc Duration in milliseconds for the Bookmark and Show Text Overlay. */
    int durationMs = kDefaultFixedDurationMs;

    /**%apidoc
     * Generic additional Resource List.
     * - For "Show On Alarm Layout": contains the users.
     * - For "Acknowledge": contains the user that confirms the popup.
     */
    std::vector<QnUuid> additionalResources;

    /**%apidoc When set, signals that all users must be targeted by the Action. */
    bool allUsers = false;

    /**%apidoc For "Alarm Layout" - if it must be opened immediately. */
    bool forced = true;

    /**%apidoc For "Exec PTZ" - id of the preset Action. */
    QString presetId;

    /**%apidoc For "Alarm Layout" - if the source Resource should also be used. */
    bool useSource = false;

    static constexpr int kDefaultRecordBeforeMs = 1000;

    /**%apidoc
     * Bookmark start time is adjusted to the left by this value in milliseconds. Also used by
     * "Fullscreen", "Open Layout" and "Show on Alarm Layout" Actions to adjust the playback time.
     */
    int recordBeforeMs = kDefaultRecordBeforeMs;

    /**%apidoc Whether a text must be pronounced. */
    bool playToClient = true;

    /**%apidoc For "Exec HTTP" Action. */
    QString contentType;

    /**%apidoc:enum
     * HTTP authentication type.
     * %value authBasicAndDigest,
     * %value authDigest,
     * %value authBasic
     */
    nx::network::http::AuthType authType = nx::network::http::AuthType::authBasicAndDigest;

    /**%apidoc HTTP method (empty string means auto-detection). */
    QString httpMethod;

    bool operator==(const ActionParameters& other) const = default;

    /**
     * \returns                        Whether all parameters have default values.
     */
    bool isDefault() const;

    /**
      * Checks if action requires confirmation according to specified target event type
      * and needConfirmation field
      */
    bool requireConfirmation(EventType targetEventType) const;
};

#define ActionParameters_Fields (needConfirmation)(actionResourceId)\
    (url)(emailAddress)(fps)(streamQuality)(recordAfter)(relayOutputId)(sayText)(tags)(text)\
    (durationMs)(additionalResources)(allUsers)(forced)(presetId)(useSource)(recordBeforeMs)\
    (playToClient)(contentType)(actionId)(authType)(httpMethod)

/* Backward compatibility is not really important here as this class is stored in the DB as json. */
QN_FUSION_DECLARE_FUNCTIONS(ActionParameters, (ubjson)(json)(xml)(csv_record), NX_VMS_COMMON_API);

NX_REFLECTION_INSTRUMENT(ActionParameters, ActionParameters_Fields)

} // namespace event
} // namespace vms
} // namespace nx
