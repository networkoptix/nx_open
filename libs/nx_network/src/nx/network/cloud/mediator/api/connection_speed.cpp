#include "connection_speed.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace hpm {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
	(ConnectionSpeed),
	(json),
	_Fields);

} // namespace api
} // namespace hpm
} // namespace nx