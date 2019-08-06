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

std::string NodeInfo::toString() const
{
    return QJson::serialized(*this).constData();
}


bool Node::operator==(const Node& right) const
{
    return nodeId == right.nodeId;
}

bool Node::operator<(const Node& right) const
{
    return nodeId < right.nodeId;
}

std::string Node::toString() const
{
    return NodeSerialization::serialized(*this).constData();
}

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
static constexpr char kPublicIpAddress[] = "publicIpAddress";
static constexpr char kUrls[] = "urls";
static constexpr char kExpirationTime[] = "expirationTime";
static constexpr char kInfoJson[] = "infoJson";
static constexpr char kCountryCode[] = "countryCode";

QString toString(const Node::time_point& expirationTime)
{
    return nx::network::http::formatDateTime(toDateTime(expirationTime));
}

Node::time_point toTimePoint(const QDateTime& dt)
{
    return Node::time_point() + std::chrono::milliseconds(dt.toMSecsSinceEpoch());
}

QJsonArray toJsonArray(const std::vector<std::string>& urls)
{
    QJsonArray result;
    for (const auto& url : urls)
        result.append(url.c_str());
    return result;
}

std::vector<std::string> toVector(const QStringList& urls)
{
    std::vector<std::string> result(urls.size());
    std::transform(urls.begin(), urls.end(), result.begin(),
        [](const QString& str) { return str.toStdString(); });
    return result;
}

QJsonObject toJsonObject(const Node& node)
{
    return QJsonObject({
        {kNodeId, node.nodeId.c_str()},
        {kPublicIpAddress, node.publicIpAddress.c_str()},
        {kUrls, toJsonArray(node.urls)},
        {kExpirationTime, toString(node.expirationTime)},
        {kInfoJson, node.infoJson.c_str()},
        {kCountryCode, node.countryCode.c_str()} });
}

Node toNode(const QVariantMap& map, const Node& defaultValue, bool* ok = nullptr)
{
    if (ok)
        *ok = false;

    Node node;
    QVariantMap::const_iterator it;

    const auto contains =
        [&map, &it](const char* value)
        {
            it = map.find(value);
            return it != map.end();
        };

    if (!contains(kNodeId))
        return defaultValue;
    node.nodeId = it->toString().toStdString();

    if (!contains(kPublicIpAddress))
        return defaultValue;
    node.publicIpAddress = it->toString().toStdString();

    if (!contains(kUrls) || !it->canConvert(QVariant::StringList))
        return defaultValue;
    node.urls = toVector(it->toStringList());

    if (!contains(kExpirationTime))
        return defaultValue;
    QDateTime dt = nx::network::http::parseDate(it->toByteArray());
    if (!dt.isValid())
        return defaultValue;
    node.expirationTime = toTimePoint(dt);

    if (!contains(kInfoJson))
        return defaultValue;
    node.infoJson = it->toString().toStdString();

    if (!contains(kCountryCode))
        return defaultValue;
    node.countryCode = it->toString().toStdString();

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

    return toNode(document.toVariant().value<QVariantMap>(), defaultValue, ok);
}

QByteArray serialized(const Node& node)
{
    return QJsonDocument(toJsonObject(node)).toJson(QJsonDocument::Compact);
}

std::vector<Node> deserialized(
    const QByteArray& json,
    const std::vector<Node>& defaultValue,
    bool* ok)
{
    if (ok)
        *ok = false;

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
        return defaultValue;

    std::vector<Node> nodes;

    QJsonArray jsonArray = document.toVariant().value<QJsonArray>();
    for (auto it = jsonArray.begin(); it != jsonArray.end(); ++it)
    {
        if (!it->isObject())
            return {};

        // Use local bool instead of ok because ok could be null, but we want to return early
        // if one json node is malformed.
        bool success = false;
        nodes.emplace_back(toNode(it->toObject().toVariantMap(), Node(), &success));
        if (!success)
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
        jsonArray.append(toJsonObject(node));
    return QJsonDocument(jsonArray).toJson(QJsonDocument::Compact);
}

} // namespace NodeSerialization

} // namespace nx::cloud::discovery
