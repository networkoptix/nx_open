#include "api_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

bool ApiIdData::operator==(const ApiIdData& rhs) const
{
    return id == rhs.id;
}

#define DB_TYPES \
    (ApiIdData) \
    (ApiDataWithVersion) \
    (ApiDatabaseDumpData) \
    (ApiDatabaseDumpToFileData) \
    (ApiSyncMarkerRecord) \
    (ApiUpdateSequenceData)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    DB_TYPES,
    (ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace ec2
