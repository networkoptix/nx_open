#pragma once

#include <cassert>
#include <typeinfo>

#include <boost/optional.hpp>

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <nx/fusion/serialization/serialization.h>

#include "json_fwd.h"

class QnJsonSerializer;

namespace QJsonDetail {

void serialize_json(const QJsonValue& value, QByteArray* outTarget,
    QJsonDocument::JsonFormat format = QJsonDocument::Compact);

bool deserialize_json(const QByteArray& value, QJsonValue* outTarget);

struct StorageInstance
{
    QnSerializerStorage<QnJsonSerializer>* operator()() const;
};

QJsonObject::const_iterator findField(
    const QJsonObject& jsonObject,
    const QString& fieldName,
    DeprecatedFieldNames* deprecatedFieldNames,
    const std::type_info& structTypeInfo);

} // namespace QJsonDetail

class QnJsonContext: public QnSerializationContext<QnJsonSerializer>
{
public:
    bool areSomeFieldsNotFound() const { return m_someFieldsNotFound; }
    void setSomeFieldsNotFound(bool value) { m_someFieldsNotFound = value; }
private:
    bool m_someFieldsNotFound{false};
};

class QnJsonSerializer:
    public QnContextSerializer<QJsonValue, QnJsonContext>,
    public QnStaticSerializerStorage<QnJsonSerializer, QJsonDetail::StorageInstance>
{
    typedef QnContextSerializer<QJsonValue, QnJsonContext> base_type;
public:
    QnJsonSerializer(int type): base_type(type) {}
};

template<class T>
class QnDefaultJsonSerializer: public QnDefaultContextSerializer<T, QnJsonSerializer>
{
};

namespace QJson {

inline QByteArray serialize(const QJsonValue &value)
{
    QByteArray result;
    QJsonDetail::serialize_json(value, &result);
    return result;
}

/**
 * Serialize the given value into intermediate JSON representation.
 * @param ctx JSON context to use.
 * @param value Value to serialize.
 * @param[out] outTarget Target JSON value, must not be nullptr.
 */
template<class T>
void serialize(QnJsonContext* ctx, const T& value, QJsonValue* outTarget)
{
    QnSerialization::serialize(ctx, value, outTarget);
}

template<class T>
void serialize(QnJsonContext* ctx, const T& value, QJsonValueRef* outTarget)
{
    NX_ASSERT(outTarget);

    QJsonValue jsonValue;
    QnSerialization::serialize(ctx, value, &jsonValue);
    *outTarget = jsonValue;
}

template<class T>
void serialize(QnJsonContext* ctx, const T& value, const QString& key, QJsonObject* outTarget)
{
    NX_ASSERT(outTarget);

    QJsonValueRef jsonValue = (*outTarget)[key];
    QJson::serialize(ctx, value, &jsonValue);
}

/**
 * Serialize the given value into a JSON string.
 * @param ctx JSON context to use.
 * @param value Value to serialize.
 * @param[out] outTarget Target JSON string, must not be nullptr.
 */
template<class T>
void serialize(QnJsonContext* ctx, const T& value, QByteArray* outTarget)
{
    NX_ASSERT(outTarget);

    QJsonValue jsonValue;
    QJson::serialize(ctx, value, &jsonValue);
    QJsonDetail::serialize_json(jsonValue, outTarget);
}

template<class T>
void serialize(const T& value, QJsonValue* outTarget)
{
    QnJsonContext ctx;
    QJson::serialize(&ctx, value, outTarget);
}

template<class T>
void serialize(const T& value, QJsonValueRef* outTarget)
{
    QnJsonContext ctx;
    QJson::serialize(&ctx, value, outTarget);
}

template<class T>
void serialize(const T& value, const QString& key, QJsonObject* outTarget)
{
    QnJsonContext ctx;
    QJson::serialize(&ctx, value, key, outTarget);
}

template<class T>
void serialize(const T& value, QByteArray* outTarget)
{
    QnJsonContext ctx;
    QJson::serialize(&ctx, value, outTarget);
}

/**
 * Deserialize the given intermediate representation of a JSON object.
 * @param ctx JSON context to use.
 * @param value Intermediate JSON representation to deserialize.
 * @param[out] outTarget Deserialization target, must not be nullptr.
 * @return Whether the deserialization was successful.
 */
template<class T>
bool deserialize(QnJsonContext* ctx, const QJsonValue& value, T* outTarget)
{
    return QnSerialization::deserialize(ctx, value, outTarget);
}

template<class T>
bool deserialize(QnJsonContext* ctx, const QJsonValueRef& value, T* outTarget)
{
    return QnSerialization::deserialize(ctx, static_cast<QJsonValue>(value), outTarget);
}

template<class T>
bool deserialize(
    QnJsonContext* ctx,
    const QJsonObject& value,
    const QString& key,
    T* outTarget,
    bool optional = false,
    bool* outFound = nullptr,
    DeprecatedFieldNames* deprecatedFieldNames = nullptr,
    const std::type_info& structTypeInfo = typeid(std::nullptr_t))
{
    QJsonObject::const_iterator pos = QJsonDetail::findField(
        value, key, deprecatedFieldNames, structTypeInfo);
    if (pos == value.end())
    {
        if (outFound != nullptr)
            *outFound = false;
        return optional;
    }

    if (outFound != nullptr)
        *outFound = true;

    bool ok = QJson::deserialize(ctx, *pos, outTarget);
    if (!ok && !optional)
    {
        qCritical() << QString(QLatin1String(
            "Can't deserialize field \"%1\" from value \"%2\""))
            .arg(key).arg(pos.value().toString());
    }
    return ok || optional;
}

/**
 * Deserialize a value from a JSON string.
 * @param ctx JSON context to use.
 * @param value JSON string to deserialize.
 * @param outTarget Deserialization target, must not be null.
 * @return Whether the deserialization was successful.
 */
template<class T>
bool deserialize(QnJsonContext* ctx, const QByteArray& value, T* outTarget)
{
    NX_ASSERT(outTarget);

    QJsonValue jsonValue;
    if (!QJsonDetail::deserialize_json(value, &jsonValue))
        return false;

    return QJson::deserialize(ctx, jsonValue, outTarget);
}

template<class T>
bool deserialize(const QJsonValue& value, T* outTarget)
{
    QnJsonContext ctx;
    return QJson::deserialize(&ctx, value, outTarget);
}

template<class T>
bool deserialize(const QJsonValueRef& value, T* outTarget)
{
    QnJsonContext ctx;
    return QJson::deserialize(&ctx, value, outTarget);
}

template<class T>
bool deserialize(const QJsonObject& value, const QString& key, T* outTarget, bool optional = false)
{
    QnJsonContext ctx;
    return QJson::deserialize(&ctx, value, key, outTarget, optional);
}

template<class T>
bool deserialize(const QByteArray& value, T* outTarget)
{
    QnJsonContext ctx;
    return QJson::deserialize(&ctx, value, outTarget);
}

/**
 * NOTE: This overload is required since otherwise QString will be converted to QJsonValue,
 * corresponding overload will be called and deserialize will fail.
 */
template<class T>
bool deserialize(const QString& value, T* outTarget)
{
    return deserialize(value.toUtf8(), outTarget);
}

/**
 * Deserialize a value from a JSON utf-8-encoded string, allowing for omitted values.
 * @param value JSON string to deserialize.
 * @param outIncompleteJsonValue Returned when some values were omitted.
 * @return Whether no deserialization errors occurred, except for possible omitted values.
 */
template<class T>
bool deserializeAllowingOmittedValues(const QByteArray& value, T* target,
    boost::optional<QJsonValue>* outIncompleteJsonValue)
{
    QJsonValue jsonValue;
    if(!QJsonDetail::deserialize_json(value, &jsonValue))
        return false;
    QnJsonContext ctx;

    bool result = QJson::deserialize(&ctx, jsonValue, target);

    if (ctx.areSomeFieldsNotFound())
        *outIncompleteJsonValue = jsonValue;
    else
        *outIncompleteJsonValue = boost::none;

    return result;
}

/**
 * Serialize the given value into a JSON string and returns it in the utf-8 format.
 * @param value Value to serialize.
 * @return Result JSON data.
 */
template<class T>
QByteArray serialized(const T& value)
{
    QByteArray result;
    QJson::serialize(value, &result);
    return result;
}

/**
 * Deserialize a value from a JSON utf-8-encoded string.
 * @param value JSON string to deserialize.
 * @param defaultValue Value to return in case of deserialization failure.
 * @param[out] success Deserialization status.
 * @return Deserialization target.
 */
template<class T>
T deserialized(const QByteArray& value, const T& defaultValue = T(), bool* success = nullptr)
{
    T target;
    const bool result = QJson::deserialize(value, &target);
    if (success)
        *success = result;

    if (result)
    {
        T local; // Enforcing NRVO, which is blocked by address-taking operator above.
        std::swap(local, target);
        return local;
    }

    return defaultValue;
}

} // namespace QJson
