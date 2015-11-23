#ifndef BUSINESS_ACTION_PARAMETERS_H
#define BUSINESS_ACTION_PARAMETERS_H

#include <business/business_fwd.h>
#include <utils/common/model_functions_fwd.h>

#include <utils/common/uuid.h>

struct QnBusinessActionParameters {
    QnBusinessActionParameters();

    /** Additional parameter for event log convenience. Does not filled when the action really occurs. */
    QnUuid actionResourceId;

    // Play Sound
    QString soundUrl;

    // Email
    QString emailAddress;

    // Popups and System Health
    QnBusiness::UserGroup userGroup;

    // Recording
    int fps;
    Qn::StreamQuality streamQuality;
    int recordingDuration;
    int recordAfter;

    // Camera Output
    QString relayOutputId;
    int relayAutoResetTimeout;
    QString inputPortId;

    // Say Text
    QString sayText;

    // Bookmark
    QString tags;

    // Generic text: Show Text Overlay
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

    /**
     * \returns                        Whether all parameters have default values.
     */
    bool isDefault() const;
};

#define QnBusinessActionParameters_Fields (actionResourceId)(soundUrl)(emailAddress)(userGroup)(fps)(streamQuality)(recordingDuration)(recordAfter)\
    (relayOutputId)(relayAutoResetTimeout)(inputPortId)(sayText)(tags)(text)(durationMs)(additionalResources)(forced)(presetId)(useSource)

QN_FUSION_DECLARE_FUNCTIONS(QnBusinessActionParameters, (ubjson)(json)(eq)(xml)(csv_record));

#endif // BUSINESS_ACTION_PARAMETERS_H
