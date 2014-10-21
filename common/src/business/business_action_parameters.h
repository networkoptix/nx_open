#ifndef BUSINESS_ACTION_PARAMETERS_H
#define BUSINESS_ACTION_PARAMETERS_H

#include <business/business_fwd.h>
#include <utils/common/model_functions_fwd.h>

//TODO: #rvasilenko adding new parameter is TOO complex. Structure can be simplified --gdm

struct QnBusinessActionParameters {
    enum Param {
        SoundUrlParam,
        EmailAddressParam,
        UserGroupParam,
        FpsParam,
        QualityParam,
        DurationParam,
        RecordBeforeParam,
        RecordAfterParam,
        RelayOutputIdParam,
        RelayAutoResetTimeoutParam,
        InputPortIdParam,
        KeyParam,
        SayTextParam,

        ParamCount
    };

    QnBusinessActionParameters();

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

    // Aggregation
    //QString keyParam;

    // convert/serialize/deserialize functions

    static QnBusinessActionParameters unpack(const QByteArray& value);
    QByteArray pack() const;

    /** 
     * \returns                        Whether all parameters have default values. 
     */
    bool isDefault() const;
};

#define QnBusinessActionParameters_Fields (soundUrl)(emailAddress)(userGroup)(fps)(streamQuality)(recordingDuration)(recordAfter)\
    (relayOutputId)(relayAutoResetTimeout)(inputPortId)(sayText)

QN_FUSION_DECLARE_FUNCTIONS(QnBusinessActionParameters, (ubjson)(json)(eq));

QByteArray serializeBusinessParams(const QnBusinessParams& value);
QnBusinessParams deserializeBusinessParams(const QByteArray& value);

#endif // BUSINESS_ACTION_PARAMETERS_H
