#include "connection_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ConnectionData),
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx

// Skip serialization of the client info data in this structure.
void serialize_field(const nx::vms::api::ClientInfoData &, QVariant *) { return; }
void deserialize_field(const QVariant &, nx::vms::api::ClientInfoData*) { return; }
