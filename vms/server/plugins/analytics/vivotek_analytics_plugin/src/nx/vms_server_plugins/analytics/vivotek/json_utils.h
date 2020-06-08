#pragma once

#include <type_traits>

#include <nx/utils/log/log_message.h>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

namespace nx::vms_server_plugins::analytics::vivotek {

QJsonValue parseJson(const QByteArray& bytes);
QByteArray serializeJson(const QJsonValue& json);

namespace json_utils_detail {

extern const QString rootPath;


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
    get(value, NX_FMT("%1.%2", path, key), json[key], args...);
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
    get(value, NX_FMT("%1[%2]", path, index), json[index], args...);
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
    -> decltype(json_utils_detail::get(value, json_utils_detail::rootPath, args...))
{
    return json_utils_detail::get(value, json_utils_detail::rootPath, args...);
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
    -> decltype((void)json_utils_detail::get(std::declval<Type*>(), json_utils_detail::rootPath, args...), Type{})
{
    Type value;
    json_utils_detail::get(&value, json_utils_detail::rootPath, args...);
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
    -> decltype(json_utils_detail::set(json_utils_detail::rootPath, args...))
{
    return json_utils_detail::set(json_utils_detail::rootPath, args...);
}

} // namespace nx::vms_server_plugins::analytics::vivotek
