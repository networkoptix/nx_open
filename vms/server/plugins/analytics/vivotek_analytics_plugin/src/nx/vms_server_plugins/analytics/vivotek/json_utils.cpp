#include "json_utils.h"

#include <cmath>
#include <climits>
#include <algorithm>

#include <nx/utils/log/assert.h>

#include <QtCore/QJsonDocument>
#include <stdexcept>

#include "exception.h"

namespace nx::vms_server_plugins::analytics::vivotek {


const QString rootJsonPath = "$";

namespace json_utils_detail {


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


JsonObject::JsonObject(const QJsonObject& object):
    QJsonObject(object)
{
}

JsonObject& JsonObject::operator=(const QJsonObject& object)
{
    QJsonObject::operator=(object);
    return *this;
}

JsonValueRef JsonObject::operator[](const QString& key)
{
    JsonValueRef valueRef(QJsonObject::operator[](key));
    valueRef.path = NX_FMT("%1.%2", path, key);
    return valueRef;
}

JsonValue JsonObject::operator[](const QString& key) const
{
    JsonValue value(QJsonObject::operator[](key));
    value.path = NX_FMT("%1.%2", path, key);
    return value;
}

JsonArray::JsonArray(const QJsonArray& array):
    QJsonArray(array)
{
}

JsonArray& JsonArray::operator=(const QJsonArray& array)
{
    QJsonArray::operator=(array);
    return *this;
}

JsonValueRef JsonArray::operator[](int index)
{
    JsonValueRef valueRef(QJsonArray::operator[](index));
    valueRef.path = NX_FMT("%1[%2]", path, index);
    return valueRef;
}

JsonValue JsonArray::operator[](int index) const
{
    JsonValue value(QJsonArray::operator[](index));
    value.path = NX_FMT("%1[%2]", path, index);
    return value;
}

JsonValue JsonArray::at(int index) const
{
    JsonValue value(QJsonArray::at(index));
    value.path = NX_FMT("%1[%2]", path, index);
    return value;
}

JsonValue::JsonValue(const QJsonValue& value):
    QJsonValue(value)
{
}

JsonValue::JsonValue(const JsonObject& object):
    QJsonValue(object),
    path(object.path)
{
}

JsonValue::JsonValue(const JsonArray& array):
    QJsonValue(array),
    path(array.path)
{
}

JsonValue::JsonValue(const JsonValueRef& valueRef):
    QJsonValue(valueRef),
    path(valueRef.path)
{
}

JsonValue& JsonValue::operator=(const QJsonValue& value)
{
    QJsonValue::operator=(value);
    return *this;
}

JsonValue& JsonValue::operator=(const JsonObject& object)
{
    QJsonValue::operator=(object);
    path = object.path;
    return *this;
}

JsonValue& JsonValue::operator=(const JsonArray& array)
{
    QJsonValue::operator=(array);
    path = array.path;
    return *this;
}

JsonValue& JsonValue::operator=(const JsonValueRef& valueRef)
{
    QJsonValue::operator=(valueRef);
    path = valueRef.path;
    return *this;
}

JsonValue JsonValue::operator[](const QString& key) const
{
    return to<JsonObject>()[key];
}

JsonValue JsonValue::operator[](int index) const
{
    return to<JsonArray>()[index];
}

JsonValue JsonValue::at(int index) const
{
    return to<JsonArray>().at(index);
}

void JsonValue::to(bool* value) const
{
    if (!isBool())
        throw Exception("%1 is not a boolean", path);

    *value = toBool();
}

void JsonValue::to(int* value) const
{
    auto doubleValue = to<double>();
    double roundedDoubleValue = std::round(doubleValue);
    if (roundedDoubleValue != doubleValue)
        throw Exception("%1 is not an integer", path);
    if (roundedDoubleValue < INT_MIN || INT_MAX < roundedDoubleValue)
        throw Exception("%1 is out of int range", path);

    *value = roundedDoubleValue;
}

void JsonValue::to(double* value) const
{
    if (!isDouble())
        throw Exception("%1 is not a number", path);

    *value = toDouble();
}

void JsonValue::to(QString* value) const
{
    if (!isString())
        throw Exception("%1 is not a string", path);

    *value = toString();
}

void JsonValue::to(JsonObject* value) const
{
    if (!isObject())
        throw Exception("%1 is not an object", path);

    *value = toObject();
    value->path = path;
}

void JsonValue::to(JsonArray* value) const
{
    if (!isArray())
        throw Exception("%1 is not an array", path);

    *value = toArray();
    value->path = path;
}

JsonValueRef::JsonValueRef(QJsonValueRef valueRef):
    QJsonValueRef(valueRef)
{
}

JsonValueRef& JsonValueRef::operator=(QJsonValueRef valueRef)
{
    QJsonValueRef::operator=(valueRef);
    return *this;
}

JsonValue parseJson(const QByteArray& bytes)
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

} // namespace nx::vms_server_plugins::analytics::vivotek
