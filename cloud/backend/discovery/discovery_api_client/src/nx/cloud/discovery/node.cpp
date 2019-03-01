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
    if (timePoint == Node::time_point())
        return {};

    using namespace std::chrono;

    QDateTime dt;
    dt.setTimeSpec(Qt::UTC);
    dt.setMSecsSinceEpoch(
        duration_cast<milliseconds>(timePoint.time_since_epoch()).count());
    return dt;
}

//-------------------------------------------------------------------------------------------------

namespace NodeSerialization {

namespace {

static constexpr char kNodeId[] = "nodeId";
static constexpr char kHost[] = "host";
static constexpr char kExpirationTime[] = "expirationTime";
static constexpr char kInfoJson[] = "infoJson";

QDateTime toDateTime(QByteArray expirationTime)
{
    if (expirationTime.endsWith(" GMT"))
        expirationTime.truncate(expirationTime.size() - 4);
    QDateTime dt = QDateTime::fromString(expirationTime, "ddd, dd MMM yyyy hh:mm:ss");
    dt.setTimeSpec(Qt::UTC);
    return dt;
}

QString toString(const Node::time_point& expirationTime)
{
    using namespace std::chrono;
    QDateTime dt;
    dt.setTimeSpec(Qt::UTC);
    dt.setMSecsSinceEpoch(
        time_point_cast<milliseconds>(expirationTime).time_since_epoch().count());
    return nx::network::http::formatDateTime(dt);
}

QJsonObject serialize(const Node& node)
{
    QJsonObject object;
    object.insert(kNodeId, node.nodeId.c_str());
    object.insert(kHost, node.host.c_str());
    object.insert(kExpirationTime, toString(node.expirationTime));
    object.insert(kInfoJson, node.infoJson.c_str());
    return object;
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
    QDateTime dt = toDateTime(it->toByteArray());
    if (!dt.isValid())
        return defaultValue;
    node.expirationTime = Node::time_point() + std::chrono::milliseconds(dt.toMSecsSinceEpoch());

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
