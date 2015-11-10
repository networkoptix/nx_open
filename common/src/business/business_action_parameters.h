#ifndef BUSINESS_ACTION_PARAMETERS_H
#define BUSINESS_ACTION_PARAMETERS_H

#include <business/business_fwd.h>
#include <utils/common/model_functions_fwd.h>

struct QnBusinessActionParameters {
    QnBusinessActionParameters();

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
    int bookmarkDuration;

    /** 
     * \returns                        Whether all parameters have default values. 
     */
    bool isDefault() const;
};

#define QnBusinessActionParameters_Fields (actionResourceId)(soundUrl)(emailAddress)(userGroup)(fps)(streamQuality)(recordingDuration)(recordAfter)\
    (relayOutputId)(relayAutoResetTimeout)(inputPortId)(sayText)(tags)(bookmarkDuration)

QN_FUSION_DECLARE_FUNCTIONS(QnBusinessActionParameters, (ubjson)(json)(eq)(xml)(csv_record));

#endif // BUSINESS_ACTION_PARAMETERS_H
