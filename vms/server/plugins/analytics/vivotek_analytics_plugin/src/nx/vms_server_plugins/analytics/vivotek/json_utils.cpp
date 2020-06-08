#include "json_utils.h"

#include <cmath>
#include <climits>

#include <nx/utils/log/assert.h>

#include <QtCore/QJsonDocument>
#include <stdexcept>

#include "exception.h"

namespace nx::vms_server_plugins::analytics::vivotek {

QJsonValue parseJson(const QByteArray& bytes)
{
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(bytes, &error);
    if (document.isObject())
        return document.object();
    if (document.isArray())
        return document.array();
    throw Exception("Failed to parse json: %1", error.errorString());
}

QByteArray serializeJson(const QJsonValue& json)
{
    QJsonDocument document;
    if (json.isObject())
        document.setObject(json.toObject());
    else if (json.isArray())
        document.setArray(json.toArray());
    else if (!NX_ASSERT(json.isObject() || json.isArray()))
        throw std::invalid_argument("Can only serialize object or array");
    return document.toJson(QJsonDocument::Compact);
}


namespace json_utils_detail {

const QString rootPath = "$";


void get(QJsonValue* value, const QString& /*path*/, const QJsonValue& json)
{
    *value = json;
}

void get(bool* value, const QString& path, const QJsonValue& json)
{
    if (!json.isBool())
        throw Exception("%1 is not a boolean", path);
    *value = json.toBool();
}

void get(double* value, const QString& path, const QJsonValue& json)
{
    if (!json.isDouble())
        throw Exception("%1 is not a number", path);
    *value = json.toDouble();
}

void get(int* value, const QString& path, const QJsonValue& json)
{
    double doubleValue;
    get(&doubleValue, path, json);
    double roundedDoubleValue = std::round(doubleValue);
    if (roundedDoubleValue != doubleValue)
        throw Exception("%1 is not an integer", path);
    if (roundedDoubleValue < INT_MIN || INT_MAX < roundedDoubleValue)
        throw Exception("%1 is out of int range", path);
    *value = roundedDoubleValue;
}

void get(QString* value, const QString& path, const QJsonValue& json)
{
    if (!json.isString())
        throw Exception("%1 is not a string", path);
    *value = json.toString();
}

void get(QJsonObject* value, const QString& path, const QJsonValue& json)
{
    if (!json.isObject())
        throw Exception("%1 is not an object", path);
    *value = json.toObject();
}

void get(QJsonArray* value, const QString& path, const QJsonValue& json)
{
    if (!json.isArray())
        throw Exception("%1 is not an array", path);
    *value = json.toArray();
}

} // namespace json_utils_detail

} // namespace nx::vms_server_plugins::analytics::vivotek
