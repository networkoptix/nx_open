#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

namespace nx::vms_server_plugins::analytics::vivotek {

struct JsonObject;
struct JsonArray;
struct JsonValue;
struct JsonValueRef;

struct JsonObject: QJsonObject
{
    QString path = "$";

    using QJsonObject::QJsonObject;
    JsonObject() = default;
    JsonObject(const QJsonObject& object);

    using QJsonObject::operator=;
    JsonObject& operator=(const QJsonObject& object);

    JsonValueRef operator[](const QString& key);
    JsonValue operator[](const QString& key) const;
};

struct JsonArray: QJsonArray
{
    QString path = "$";

    using QJsonArray::QJsonArray;
    JsonArray() = default;
    JsonArray(const QJsonArray& array);

    using QJsonArray::operator=;
    JsonArray& operator=(const QJsonArray& array);

    JsonValueRef operator[](int index);
    JsonValue operator[](int index) const;
    JsonValue at(int index) const;
};

struct JsonValue: QJsonValue
{
    QString path = "$";

    using QJsonValue::QJsonValue;
    JsonValue() = default;
    JsonValue(const QJsonValue& value);
    JsonValue(const JsonObject& object);
    JsonValue(const JsonArray& array);
    JsonValue(const JsonValueRef& valueRef);

    using QJsonValue::operator=;
    JsonValue& operator=(const QJsonValue& value);
    JsonValue& operator=(const JsonObject& object);
    JsonValue& operator=(const JsonArray& array);
    JsonValue& operator=(const JsonValueRef& valueRef);

    JsonValue operator[](const QString& key) const;
    JsonValue operator[](int index) const;
    JsonValue at(int index) const;

    void to(bool* value) const;
    void to(int* value) const;
    void to(float* value) const;
    void to(double* value) const;
    void to(QString* value) const;
    void to(JsonObject* value) const;
    void to(JsonArray* value) const;

    template <typename Value>
    Value to() const
    {
        Value value;
        to(&value);
        return value;
    }
};

struct JsonValueRef: QJsonValueRef
{
    QString path = "$";

    using QJsonValueRef::QJsonValueRef;
    JsonValueRef(QJsonValueRef valueRef);

    using QJsonValueRef::operator=;
    JsonValueRef& operator=(QJsonValueRef valueRef);

    template <typename Key>
    JsonValue operator[](const Key& key) const
    {
        return JsonValue(*this)[key];
    }

    template <typename Key>
    JsonValue at(const Key& key) const
    {
        return JsonValue(*this).at(key);
    }

    template <typename Value>
    void to(Value* value) const
    {
        JsonValue(*this).to(value);
    }

    template <typename Value>
    Value to() const
    {
        return JsonValue(*this).to<Value>();
    }
};

JsonValue parseJson(const QByteArray& bytes);
QByteArray serializeJson(const QJsonValue& json);

} // namespace nx::vms_server_plugins::analytics::vivotek
