#include "updates2_status_data.h"
#include <nx/fusion/model_functions.h>

namespace nx {
namespace api {

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Updates2StatusData, StatusCode)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((Updates2StatusData), (json), _Fields)

} // namespace api
} // namespace nx
