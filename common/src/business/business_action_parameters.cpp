#include "business_action_parameters.h"

#include <utils/common/model_functions.h>

namespace
{
    const int kDefaultFixedDurationMs = 5000;
    const int kDefaultRecordBeforeMs = 1000;
}

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
    , durationMs(kDefaultFixedDurationMs)
    , additionalResources()
    , forced(true)
    , presetId()
    , useSource(false)
    , recordBeforeMs(kDefaultRecordBeforeMs)
    , playToClient(true)
{}


bool QnBusinessActionParameters::isDefault() const
{
    static QnBusinessActionParameters empty;
    return (*this) == empty;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnBusinessActionParameters, (ubjson)(json)(eq)(xml)(csv_record), QnBusinessActionParameters_Fields, (optional, true) )
