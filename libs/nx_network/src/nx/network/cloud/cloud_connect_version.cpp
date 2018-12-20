#include "cloud_connect_version.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::hpm::api, CloudConnectVersion,
    (nx::hpm::api::CloudConnectVersion::initial, "initial")
    (nx::hpm::api::CloudConnectVersion::tryingEveryAddressOfPeer,
        "tryingEveryAddressOfPeer")
    (nx::hpm::api::CloudConnectVersion::serverChecksConnectionState,
        "serverChecksConnectionState")
    (nx::hpm::api::CloudConnectVersion::clientSupportsConnectSessionWithoutUdpEndpoints,
        "clientSupportsConnectSessionWithoutUdpEndpoints")
)
