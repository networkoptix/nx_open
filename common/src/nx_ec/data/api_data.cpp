#include "api_data.h"

#include <utils/common/model_functions.h>

namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiIdData) (ApiDataWithVersion) (ApiDatabaseDumpData) (ApiSyncMarkerRecord) (ApiUpdateSequenceData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields, (optional, true))
} // namespace ec2
