#include "api_runtime_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiRuntimeData), (ubjson)(xml)(json)(csv_record), _Fields, (optional, true))
} // namespace ec2

void serialize_field(const ec2::ApiRuntimeData &, QVariant *) { return; }
void deserialize_field(const QVariant &, ec2::ApiRuntimeData *) { return; }
