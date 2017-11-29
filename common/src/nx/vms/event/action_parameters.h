#pragma once

#include <nx/vms/event/event_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/http_types.h>

namespace nx {
namespace vms {
namespace event {

struct ActionParameters
{
    ActionParameters();

    /**
      * Identifier of action which is used in show/hide popup action.
      */
    QnUuid actionId;

    /**
      * Shows if action should be confirmed by user. Currently used for bookmarks confirmation
      */
    bool needConfirmation = false;

    /** Additional parameter for event log convenience. Does not filled when the action really occurs. */
    QnUuid actionResourceId;

    // Play Sound / exec HTTP action
    QString url;

    // Email
    QString emailAddress;

    // Recording
    int fps;
    Qn::StreamQuality streamQuality;
    //! for bookmarks this represents epsilon, bookmark end time extended by
    int recordAfter; //< Seconds.

    // Camera Output
    QString relayOutputId;

    // Say Text
    QString sayText;

    // Bookmark
    QString tags;

    /**
      * For "Show Text Overlay" action: generic text.
      * For "Exec HTTP" action: message body.
      * For "Acknowledge" action: bookmark description.
      */
    QString text;

    // Generic duration: Bookmark, Show Text Overlay
    int durationMs;

    // Generic additional resources List: Show On Alarm Layout - users
    // With acknowledgeAction contains user that confirms popup.
    std::vector<QnUuid> additionalResources;

    // When set, signalizes that all users must be targeted by the action.
    bool allUsers;

    // Alarm Layout - if it must be opened immediately
    bool forced;

    // exec PTZ preset action
    QString presetId;

    // Alarm Layout - if the source resource should also be used
    bool useSource;

    //! Bookmark start time adjusted to the left by this value
    int recordBeforeMs;

    //Say text
    bool playToClient;

    //HTTP action
    QString contentType;

    //HTTP action
    nx_http::AsyncHttpClient::AuthType authType;

    //HTTP action (empty string means auto detection)
    nx_http::Method::ValueType requestType;

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
    (playToClient)(contentType)(actionId)(authType)(requestType)

/* Backward compatibility is not really important here as this class is stored in the DB as json. */
QN_FUSION_DECLARE_FUNCTIONS(ActionParameters, (ubjson)(json)(eq)(xml)(csv_record));

} // namespace event
} // namespace vms
} // namespace nx
