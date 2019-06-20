#pragma once

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace hpm {
namespace api {

using BytesPerSecond = int;

struct ConnectionSpeed
{
	std::chrono::microseconds pingTime;
	BytesPerSecond bandwidth;
};

#define ConnectionSpeed_Fields (pingTime)(bandwidth)
QN_FUSION_DECLARE_FUNCTIONS(ConnectionSpeed, (json), NX_NETWORK_API)

} // namespace api
} // namespace hpm
} // namespace nx
