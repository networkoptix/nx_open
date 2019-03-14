#pragma once

#include <string>
#include <chrono>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::cloud::discovery {

struct NX_DISCOVERY_CLIENT_API NodeInfo
{
    std::string nodeId;
    /* The object under "info" key */
    std::string infoJson;
};

#define NodeInfo_Fields (nodeId)(infoJson)

QN_FUSION_DECLARE_FUNCTIONS(NodeInfo, (json), NX_DISCOVERY_CLIENT_API)

//-------------------------------------------------------------------------------------------------

struct NX_DISCOVERY_CLIENT_API Node
{
    std::string nodeId;
    std::string host;
    /* http time format */
    std::chrono::system_clock::time_point expirationTime;
    /* The object under "info" key */
    std::string infoJson;

    bool operator==(const Node& right) const
    {
        // TODO do other fields need to be compared? they are volatile.
        return nodeId == right.nodeId;
    }

    bool operator<(const Node& right)const
    {
        return nodeId < right.nodeId;
    }
};

#define Node_Fields (nodeId)(host)(expirationTime)(infoJson)

QN_FUSION_DECLARE_FUNCTIONS(Node, (json), NX_DISCOVERY_CLIENT_API)

} // namespace nx::cloud::discovery
