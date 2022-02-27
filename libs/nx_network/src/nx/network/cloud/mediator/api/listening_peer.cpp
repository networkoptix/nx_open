// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "listening_peer.h"

#include <nx/fusion/model_functions.h>

namespace nx::hpm::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ListeningPeer, (json), ListeningPeer_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BoundClient, (json), BoundClient_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemPeers, (json), SystemPeers_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ListeningPeers, (json), ListeningPeers_Fields)

} // namespace nx::hpm::api
