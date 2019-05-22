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
    std::vector<std::string> urls;
    /* The object under "info" key */
    std::string infoJson;
};

#define NodeInfo_Fields (nodeId)(urls)(infoJson)

QN_FUSION_DECLARE_FUNCTIONS(NodeInfo, (json), NX_DISCOVERY_CLIENT_API)

//-------------------------------------------------------------------------------------------------

struct NX_DISCOVERY_CLIENT_API Node
{
    using time_point = std::chrono::system_clock::time_point;

    std::string nodeId;
    /* The public ip address of this node as reported by the discovery service */
    std::string publicIpAddress;
    std::vector<std::string> urls;
    time_point expirationTime;
    /* A string containing discovery client specific json. */
    std::string infoJson;

    bool operator==(const Node& right) const;

    bool operator<(const Node& right)const;

    std::string toString() const;
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

