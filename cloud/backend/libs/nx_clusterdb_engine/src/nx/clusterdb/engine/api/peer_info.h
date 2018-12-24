#pragma once

#include <string>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::clusterdb::engine::api {

struct NX_DATA_SYNC_ENGINE_API PeerInfo
{
    std::string id;
};

#define PeerInfo_Fields (id)

QN_FUSION_DECLARE_FUNCTIONS(
    PeerInfo,
    (json),
    NX_DATA_SYNC_ENGINE_API)

} // namespace nx::clusterdb::engine::api
