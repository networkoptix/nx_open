#include "api_media_server_data.h"
#include "api_model_functions_impl.h"

namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiStorageData) (ApiMediaServerData) (ApiMediaServerUserAttributesData) (ApiMediaServerDataEx), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)

    QN_FUSION_DEFINE_FUNCTIONS(ApiMediaServerData, (eq))
} // namespace ec2
