#pragma once

#include <business/business_fwd.h>
#include <nx/fusion/model_functions_fwd.h>

#include <nx/utils/uuid.h>


struct QnBusinessActionParameters
{
    QnBusinessActionParameters();

    /**
      * Shows if action should be confirmed by user. Currently used for bookmarks confirmation
      */
    bool needConfirmation = false;

    QnBusiness::ActionType targetActionType = QnBusiness::UndefinedAction;


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

    // Generic text: Show Text Overlay / exec HTTP action: message body
    QString text;

    // Generic duration: Bookmark, Show Text Overlay
    int durationMs;

    // Generic additional resources List: Show On Alarm Layout - users
    std::vector<QnUuid> additionalResources;

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

    /**
     * \returns                        Whether all parameters have default values.
     */
    bool isDefault() const;
};

#define QnBusinessActionParameters_Fields (targetActionType)(needConfirmation)(actionResourceId)\
    (url)(emailAddress)(fps)(streamQuality)(recordAfter)(relayOutputId)(sayText)(tags)(text)\
    (durationMs)(additionalResources)(forced)(presetId)(useSource)(recordBeforeMs)\
    (playToClient)(contentType)

/* Backward compatibility is not really important here as this class is not stored in the DB. */
QN_FUSION_DECLARE_FUNCTIONS(QnBusinessActionParameters, (ubjson)(json)(eq)(xml)(csv_record));
