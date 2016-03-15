#include "api_reverse_connection_data.h"

#include <utils/common/model_functions.h>

namespace ec2 {

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiReverseConnectionData), \
        (ubjson)(xml)(json)(sql_record)(csv_record), _Fields, (optional, true))

} // namespace ec2
