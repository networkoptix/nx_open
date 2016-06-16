#include "api_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiIdData) (ApiDataWithVersion) (ApiDatabaseDumpData) (ApiDatabaseDumpToFileData) (ApiSyncMarkerRecord) (ApiUpdateSequenceData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields, (optional, true))
} // namespace ec2
