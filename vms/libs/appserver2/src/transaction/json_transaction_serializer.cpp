// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "json_transaction_serializer.h"

#include <nx/fusion/model_functions.h>

namespace nx::p2p {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TransportHeader, (json), (via)(dstPeers)(userIds))

} // namespace nx::p2p
