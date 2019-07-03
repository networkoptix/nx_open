#include "connection_speed.h"

#include <nx/fusion/model_functions.h>

namespace nx::hpm::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
	(ConnectionSpeed)(PeerConnectionSpeed),
	(json),
	_Fields)

} // namespace nx::hpm::api