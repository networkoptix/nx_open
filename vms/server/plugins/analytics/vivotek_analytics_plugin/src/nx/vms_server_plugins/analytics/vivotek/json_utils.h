#pragma once

#include <type_traits>

#include <nx/utils/log/log_message.h>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

namespace nx::vms_server_plugins::analytics::vivotek {


extern const QString rootJsonPath;

inline QString extendJsonPath(QString base)
{
    return base;
}

template <typename... Keys>
QString extendJsonPath(const QString& base, const QString& key, const Keys&... keys)
{
    return extendJsonPath(NX_FMT("%1.%2", base, key), keys...);
}

template <typename... Keys>
QString extendJsonPath(const QString& base, int index, const Keys&... keys)
{
    return extendJsonPath(NX_FMT("%1[%2]", base, index), keys...);
}


namespace json_utils_detail {


void get(QJsonValue* value, const QString& path, const QJsonValue& json);

void get(bool* value, const QString& path, const QJsonValue& json);
void get(double* value, const QString& path, const QJsonValue& json);
void get(int* value, const QString& path, const QJsonValue& json);
void get(QString* value, const QString& path, const QJsonValue& json);
void get(QJsonObject* value, const QString& path, const QJsonValue& json);
void get(QJsonArray* value, const QString& path, const QJsonValue& json);

template <typename Type, typename... Args>
void get(Type* value, const QString& path, const QJsonObject& json, const QString& key,
    const Args&... args);

template <typename Type, typename... Args>
void get(Type* value, const QString& path, const QJsonValue& json, const QString& key,
    const Args&... args);

template <typename Type, typename... Args>
void get(Type* value, const QString& path, const QJsonArray& json, int index,
    const Args&... args);

template <typename Type, typename... Args>
void get(Type* value, const QString& path, const QJsonValue& json, int index,
    const Args&... args);

template <typename Type, typename... Args>
void get(Type* value, const QString& path, const QJsonObject& json, const QString& key,
    const Args&... args)
{
    get(value, extendJsonPath(path, key), json[key], args...);
}

template <typename Type, typename... Args>
void get(Type* value, const QString& path, const QJsonValue& json, const QString& key,
    const Args&... args)
{
    QJsonObject object;
    get(&object, path, json);
    get(value, path, object, key, args...);
}

template <typename Type, typename... Args>
void get(Type* value, const QString& path, const QJsonArray& json, int index,
    const Args&... args)
{
    get(value, extendJsonPath(path, index), json[index], args...);
}

template <typename Type, typename... Args>
void get(Type* value, const QString& path, const QJsonValue& json, int index,
    const Args&... args)
{
    QJsonArray array;
    get(&array, path, json);
    get(value, path, array, index, args...);
}


template <typename Type>
void set(const QString& /*path*/, QJsonValue* json, const Type& value)
{
    *json = value;
}

template <typename... Args, std::enable_if_t<(sizeof...(Args) > 0)>* = nullptr>
void set(const QString& path, QJsonObject* json, const QString& key, const Args&... args);

template <typename... Args, std::enable_if_t<(sizeof...(Args) > 0)>* = nullptr>
void set(const QString& path, QJsonValue* json, const QString& key, const Args&... args);

template <typename... Args, std::enable_if_t<(sizeof...(Args) > 0)>* = nullptr>
void set(const QString& path, QJsonArray* json, int index, const Args&... args);

template <typename... Args, std::enable_if_t<(sizeof...(Args) > 0)>* = nullptr>
void set(const QString& path, QJsonValue* json, int index, const Args&... args);

template <typename... Args, std::enable_if_t<(sizeof...(Args) > 0)>*>
void set(const QString& path, QJsonObject* json, const QString& key, const Args&... args)
{
    QJsonValue subJson = (*json)[key];
    set(NX_FMT("%1.%2", path, key), &subJson, args...);
    (*json)[key] = subJson;
}

template <typename... Args, std::enable_if_t<(sizeof...(Args) > 0)>*>
void set(const QString& path, QJsonValue* json, const QString& key, const Args&... args)
{
    QJsonObject object;
    get(&object, path, *json);
    set(path, &object, key, args...);
    *json = object;
}

template <typename... Args, std::enable_if_t<(sizeof...(Args) > 0)>*>
void set(const QString& path, QJsonArray* json, int index, const Args&... args)
{
    QJsonValue subJson = (*json)[index];
    set(NX_FMT("%1[%2]", path, index), &subJson, args...);
    (*json)[index] = subJson;
}

template <typename... Args, std::enable_if_t<(sizeof...(Args) > 0)>*>
void set(const QString& path, QJsonValue* json, int index, const Args&... args)
{
    QJsonArray array;
    get(&array, path, *json);
    set(path, &array, index, args...);
    *json = array;
}

} // namespace json_utils_detail

template <typename Type, typename... Args>
auto get(Type* value, const QString& path, const Args&... args)
    -> decltype(json_utils_detail::get(value, path, args...))
{
    return json_utils_detail::get(value, path, args...);
}

template <typename Type, typename... Args>
auto get(Type* value, const Args&... args)
    -> decltype(json_utils_detail::get(value, rootJsonPath, args...))
{
    return json_utils_detail::get(value, rootJsonPath, args...);
}

template <typename Type, typename... Args>
auto get(const QString& path, const Args&... args)
    -> decltype((void)json_utils_detail::get(std::declval<Type*>(), path, args...), Type{})
{
    Type value;
    json_utils_detail::get(&value, path, args...);
    return value;
}

template <typename Type, typename... Args>
auto get(const Args&... args)
    -> decltype((void)json_utils_detail::get(std::declval<Type*>(), rootJsonPath, args...), Type{})
{
    Type value;
    json_utils_detail::get(&value, rootJsonPath, args...);
    return value;
}


template <typename... Args>
auto set(const QString& path, const Args&... args)
    -> decltype(json_utils_detail::set(path, args...))
{
    return json_utils_detail::set(path, args...);
}

template <typename... Args>
auto set(const Args&... args)
    -> decltype(json_utils_detail::set(rootJsonPath, args...))
{
    return json_utils_detail::set(rootJsonPath, args...);
}


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
