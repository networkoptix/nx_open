#include "business_action_parameters.h"

#include <utils/common/model_functions.h>

QnBusinessActionParameters::QnBusinessActionParameters() {
    userGroup = QnBusiness::EveryOne;
    fps = 10;
    streamQuality = Qn::QualityHighest;
    recordingDuration = 0;
    recordAfter = 0;
    relayAutoResetTimeout = 0;
}


bool QnBusinessActionParameters::isDefault() const
{
    static QnBusinessActionParameters empty;
    return (*this) == empty;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnBusinessActionParameters, (ubjson)(json)(eq), QnBusinessActionParameters_Fields, (optional, true) )
