#include "api_connection_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiLoginData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)

} // namespace ec2

void serialize_field(const ec2::ApiClientInfoData &, QVariant *) { return; }
void deserialize_field(const QVariant &, ec2::ApiClientInfoData *) { return; }
