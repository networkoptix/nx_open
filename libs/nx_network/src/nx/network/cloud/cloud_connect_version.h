// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::hpm::api {

/**
 * Indicates cloud connect supported features by peer.
 */
NX_REFLECTION_ENUM_CLASS(CloudConnectVersion,
    /** Used when cloudConnectionVersion attribute is missing in message */
    initial = 1,

    /** #CLOUD-398 */
    tryingEveryAddressOfPeer,

    /** #CLOUD-824 */
    serverChecksConnectionState,

    /** #VMS-8224. */
    clientSupportsConnectSessionWithoutUdpEndpoints,

    /** #CB-236. */
    connectOverHttpHasHostnameAsString,

    /** #CB-2331. */
    clientSupportsRelayConnectionTesting
)

constexpr const CloudConnectVersion kDefaultCloudConnectVersion =
    CloudConnectVersion::initial;

constexpr const CloudConnectVersion kCurrentCloudConnectVersion =
    CloudConnectVersion::clientSupportsRelayConnectionTesting;

} // nx::hpm::api
