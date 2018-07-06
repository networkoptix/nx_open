#include "updates2_action_data.h"
#include <nx/fusion/model_functions.h>

namespace nx {
namespace api {

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Updates2ActionData, ActionCode)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((Updates2ActionData), (json), _Fields)

} // namespace api
} // namespace nx
