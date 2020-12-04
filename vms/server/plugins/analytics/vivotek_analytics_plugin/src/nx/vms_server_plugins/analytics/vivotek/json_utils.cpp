#include "json_utils.h"

#include <cmath>
#include <climits>
#include <algorithm>

#include <nx/utils/log/log_message.h>
#include <nx/utils/log/assert.h>
#include <nx/vms_server_plugins/utils/exception.h>

#include <QtCore/QJsonDocument>
#include <stdexcept>

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::vms_server_plugins::utils;

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
        throw Exception("%1 is %2 while %3 is expected", path, type(), Bool);

    *value = toBool();
}

void JsonValue::to(int* value) const
{
    auto doubleValue = to<double>();
    double roundedDoubleValue = std::round(doubleValue);
    if (roundedDoubleValue != doubleValue)
        throw Exception("%1 has fractional part", path);
    if (roundedDoubleValue < INT_MIN || INT_MAX < roundedDoubleValue)
        throw Exception("%1 is out of int range", path);

    *value = roundedDoubleValue;
}

void JsonValue::to(float* value) const
{
    *value = to<double>();
}

void JsonValue::to(double* value) const
{
    if (!isDouble())
        throw Exception("%1 is %2 while %3 is expected", path, type(), Double);

    *value = toDouble();
}

void JsonValue::to(QString* value) const
{
    if (!isString())
        throw Exception("%1 is %2 while %3 is expected", path, type(), String);

    *value = toString();
}

void JsonValue::to(JsonObject* value) const
{
    if (!isObject())
        throw Exception("%1 is %2 while %3 is expected", path, type(), Object);

    *value = toObject();
    value->path = path;
}

void JsonValue::to(JsonArray* value) const
{
    if (!isArray())
        throw Exception("%1 is %2 while %3 is expected", path, type(), Array);

    *value = toArray();
    value->path = path;
}

bool JsonValue::contains(const QString& string) const
{
    if (isString())
        return to<QString>().contains(string);

    if (isObject())
        return to<JsonObject>().contains(string);

    if (isArray())
        return to<JsonArray>().contains(string);

    throw Exception("%1 is %2 while %3, %4 or %5 is expected",
        path, type(), String, Object, Array);
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

bool JsonValueRef::contains(const QString& string) const
{
    return JsonValue(*this).contains(string);
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
    else
        throw Exception("Can only serialize %1 or %2", JsonValue::Object, JsonValue::Array);

    return document.toJson(QJsonDocument::Compact);
}

} // namespace nx::vms_server_plugins::analytics::vivotek
