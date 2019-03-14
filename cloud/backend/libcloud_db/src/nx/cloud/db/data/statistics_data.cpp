#include "statistics_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace sql {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (DurationStatistics)(QueryStatistics),
    (json),
    _Fields)

} // namespace sql
} // namespace nx

namespace nx::cloud::db {
namespace data {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json),
    _data_Fields)

} // namespace data
} // namespace nx::cloud::db
