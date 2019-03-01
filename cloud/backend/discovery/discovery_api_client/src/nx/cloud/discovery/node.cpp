#include "node.h"

#include <QDateTime>
#include <QtCore/qjsondocument.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_types.h>

namespace nx::cloud::discovery {

//TODO uncomment when serialization works properly for Node,
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (NodeInfo)/*(Node)*/,
    (json),
    _Fields)

//-------------------------------------------------------------------------------------------------

QDateTime toDateTime(const Node::time_point& timePoint)
{
    QDateTime dt;
    dt.setMSecsSinceEpoch(std::chrono::duration_cast<std::chrono::milliseconds>(
        timePoint.time_since_epoch()).count());
    return dt;
}

//-------------------------------------------------------------------------------------------------

namespace NodeSerialization {

namespace {

static constexpr char kNodeId[] = "nodeId";
static constexpr char kHost[] = "host";
static constexpr char kExpirationTime[] = "expirationTime";
static constexpr char kInfoJson[] = "infoJson";

QString toString(const Node::time_point& expirationTime)
{
    return nx::network::http::formatDateTime(toDateTime(expirationTime));
}

Node::time_point toTimePoint(const QDateTime& dt)
{
    return Node::time_point() + std::chrono::milliseconds(dt.toMSecsSinceEpoch());
}

QJsonObject serialize(const Node& node)
{
    return QJsonObject({
        {kNodeId, node.nodeId.c_str()},
        {kHost, node.host.c_str()},
        {kExpirationTime, toString(node.expirationTime)},
        {kInfoJson, node.infoJson.c_str()} });
}

Node deserialize(const QVariantMap& map, const Node& defaultValue, bool* ok = nullptr)
{
    if (ok)
        *ok = false;

    Node node;

    auto it = map.find(kNodeId);
    if (it != map.end())
        node.nodeId = it->toString().toStdString();
    if (node.nodeId.empty())
        return defaultValue;

    it = map.find(kHost);
    if (it != map.end())
        node.host = it->toString().toStdString();
    if (node.host.empty())
        return defaultValue;

    it = map.find(kExpirationTime);
    if (it == map.end())
        return defaultValue;
    QDateTime dt = nx::network::http::parseDate(it->toByteArray());
    if (!dt.isValid())
        return defaultValue;
    node.expirationTime = toTimePoint(dt);

    it = map.find(kInfoJson);
    if (it != map.end())
        node.infoJson = it->toString().toStdString();
    if (node.infoJson.empty())
        return defaultValue;

    if (ok)
        *ok = true;

    return node;
}

} // namespace

Node deserialized(const QByteArray& json, const Node& defaultValue, bool* ok)
{
    if (ok)
        *ok = false;

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return defaultValue;

    return deserialize(document.toVariant().value<QVariantMap>(), defaultValue, ok);
}

QByteArray serialized(const Node& node)
{
    return QJsonDocument(serialize(node)).toJson(QJsonDocument::Compact);
}

std::vector<Node> deserialized(
    const QByteArray& json,
    const std::vector<Node>& defaultValue,
    bool * ok)
{
    if (ok)
        *ok = false;

    std::vector<Node> nodes;

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
        return defaultValue;

    QJsonArray jsonArray = document.toVariant().value<QJsonArray>();

    for (auto it = jsonArray.begin(); it != jsonArray.end(); ++it)
    {
        if (!it->isObject())
            return {};

        nodes.emplace_back(deserialize(it->toObject().toVariantMap(), Node(), ok));
        if (ok && !(*ok))
            return {};
    }

    if (ok)
        *ok = true;

    return nodes;
}

QByteArray serialized(const std::vector<Node>& nodes)
{
    QJsonArray jsonArray;
    for (const auto& node : nodes)
        jsonArray.append(serialize(node));
    return QJsonDocument(jsonArray).toJson(QJsonDocument::Compact);
}

} // namespace NodeSerialization

} // namespace nx::cloud::discovery
