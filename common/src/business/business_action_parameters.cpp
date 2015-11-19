#include "business_action_parameters.h"

#include <utils/common/model_functions.h>

QnBusinessActionParameters::QnBusinessActionParameters()
    : actionResourceId()
    , soundUrl()
    , emailAddress()
    , userGroup(QnBusiness::EveryOne)
    , fps(10)
    , streamQuality(Qn::QualityHighest)
    , recordingDuration(0)
    , recordAfter(0)
    , relayOutputId()
    , relayAutoResetTimeout(0)
    , inputPortId()
    , sayText()
    , tags()
    , text()
    , durationMs(0)
    , additionalResources()
{}


bool QnBusinessActionParameters::isDefault() const
{
    static QnBusinessActionParameters empty;
    return (*this) == empty;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnBusinessActionParameters, (ubjson)(json)(eq)(xml)(csv_record), QnBusinessActionParameters_Fields, (optional, true) )
