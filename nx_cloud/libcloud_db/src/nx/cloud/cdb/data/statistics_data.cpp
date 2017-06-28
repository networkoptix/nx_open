#include "statistics_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace utils {
namespace db {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (DurationStatistics)(QueryStatistics),
    (json),
    _Fields)

} // namespace db
} // namespace utils
} // namespace nx

namespace nx {
namespace cdb {
namespace data {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json),
    _data_Fields)

} // namespace data
} // namespace cdb
} // namespace nx
