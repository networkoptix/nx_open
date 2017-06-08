#include "business_action_parameters.h"

#include <nx/fusion/model_functions.h>

namespace
{
    const int kDefaultFixedDurationMs = 5000;
    const int kDefaultRecordBeforeMs = 1000;
}

QnBusinessActionParameters::QnBusinessActionParameters()
    : actionResourceId()
    , url()
    , emailAddress()
    , userGroup(QnBusiness::EveryOne)
    , fps(10)
    , streamQuality(Qn::QualityHighest)
    , recordAfter(0)
    , relayOutputId()
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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnBusinessActionParameters), (ubjson)(json)(eq)(xml)(csv_record), _Fields, (brief, true))
