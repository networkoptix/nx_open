#include "api_fwd.h"
#include "api_business_rule_data.h"

#include <utils/fusion/fusion_adaptor.h>
#include <utils/common/model_functions.h>
#include <utils/serialization/binary_functions.h>
#include <utils/serialization/json_functions.h>

namespace ec2 {

    QN_FUSION_ADAPT_STRUCT(ApiBusinessRuleData, ApiBusinessRuleData_Fields)
    QN_FUSION_ADAPT_STRUCT(ApiBusinessActionData, ApiBusinessActionData_Fields)

    QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES((ApiBusinessRuleData)(ApiBusinessActionData), (binary)(json))

} // namespace ec2

