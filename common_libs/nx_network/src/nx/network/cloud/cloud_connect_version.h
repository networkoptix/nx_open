#pragma once

namespace nx {
namespace hpm {
namespace api {

/**
 * Indicates cloud connect supported features by peer.
 */
enum class CloudConnectVersion
{
    /** Used when cloudConnectionVersion attribute is missing in message */
    initial = 1,

    /** #CLOUD-398 */
    tryingEveryAddressOfPeer,

    /** #CLOUD-824 */
    serverChecksConnectionState,

    /** #VMS-8224. */
    clientSupportsConnectSessionWithoutUdpEndpoints,
};

constexpr const CloudConnectVersion kDefaultCloudConnectVersion =
    CloudConnectVersion::initial;
constexpr const CloudConnectVersion kCurrentCloudConnectVersion =
    CloudConnectVersion::clientSupportsConnectSessionWithoutUdpEndpoints;

} // namespace api
} // namespace hpm
} // namespace nx
