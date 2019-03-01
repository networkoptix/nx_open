#pragma once

#include <string>
#include <chrono>
#include <vector>

#include <QDateTime>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_adaptor.h>

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
    using time_point = std::chrono::system_clock::time_point;

    std::string nodeId;
    std::string host;
    time_point expirationTime;
    /* The object under "info" key */
    std::string infoJson;

    bool operator==(const Node& right) const
    {
        return nodeId == right.nodeId;
    }

    bool operator<(const Node& right)const
    {
        return nodeId < right.nodeId;
    }
};

//-------------------------------------------------------------------------------------------------

QDateTime NX_DISCOVERY_CLIENT_API toDateTime(
    const std::chrono::system_clock::time_point& timePoint);

//-------------------------------------------------------------------------------------------------

//TODO figure out how to use qnfusion library to serialize Node::expirationTime correctly.
namespace NodeSerialization {

Node NX_DISCOVERY_CLIENT_API deserialized(
    const QByteArray& json,
    const Node& defaultValue = Node(),
    bool* ok = nullptr);

QByteArray NX_DISCOVERY_CLIENT_API serialized(const Node& node);


std::vector<Node> NX_DISCOVERY_CLIENT_API deserialized(
    const QByteArray& json,
    const std::vector<Node>& defaultValue,
    bool*ok = nullptr);

QByteArray NX_DISCOVERY_CLIENT_API serialized(const std::vector<Node>& nodes);

} // namespace NodeSerialization

} // namespace nx::cloud::discovery

//-------------------------------------------------------------------------------------------------

