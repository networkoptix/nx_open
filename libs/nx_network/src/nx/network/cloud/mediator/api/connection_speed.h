#pragma once

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::hpm::api {

struct NX_NETWORK_API ConnectionSpeed
{
    std::chrono::microseconds pingTime = std::chrono::microseconds(0);
	// Bandwidth in bytes per millisecond
	int bandwidth = 0;
};

#define ConnectionSpeed_Fields (pingTime)(bandwidth)

struct NX_NETWORK_API PeerConnectionSpeed
{
	std::string serverId;
	std::string systemId;
	ConnectionSpeed connectionSpeed;
};

#define PeerConnectionSpeed_Fields (serverId)(systemId)(connectionSpeed)

QN_FUSION_DECLARE_FUNCTIONS(nx::hpm::api::ConnectionSpeed, (json), NX_NETWORK_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::hpm::api::PeerConnectionSpeed, (json), NX_NETWORK_API)

} // namespace nx::hpm::api
