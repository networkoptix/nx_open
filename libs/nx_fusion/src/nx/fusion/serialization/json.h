// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cassert>
#include <optional>
#include <typeinfo>
#include <unordered_set>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <nx/fusion/serialization/serialization.h>
#include <nx/utils/buffer.h>
#include <nx/utils/exception.h>
#include <nx/utils/log/log_main.h>

#include "json_fwd.h"

class QnJsonSerializer;

template<typename Type, typename Enable = void>
struct HasDirectObjectKeySerializer: std::false_type
{
};

// Should have key serializers for all enum values
template<typename Type>
struct HasDirectObjectKeySerializer<Type, std::enable_if_t<std::is_enum_v<Type>>>: std::true_type
{
};

namespace QJsonDetail {

NX_FUSION_API void serialize_json(const QJsonValue& value, QByteArray* outTarget,
    QJsonDocument::JsonFormat format = QJsonDocument::Compact);

NX_FUSION_API bool deserialize_json(
    const QByteArray& value, QJsonValue* outTarget, QString* errorMessage = nullptr);

struct StorageInstance
{
    NX_FUSION_API QnSerializerStorage<QnJsonSerializer>* operator()() const;
};

NX_FUSION_API QJsonObject::const_iterator findField(
    const QJsonObject& jsonObject,
    const QString& fieldName,
    DeprecatedFieldNames* deprecatedFieldNames,
    const std::type_info& structTypeInfo,
    bool optional);

} // namespace QJsonDetail

class QnJsonContext: public QnSerializationContext<QnJsonSerializer>
{
public:
    bool areSomeFieldsNotFound() const { return m_someFieldsNotFound; }
    void setSomeFieldsNotFound(bool value) { m_someFieldsNotFound = value; }

    bool areSomeFieldsFound() const { return m_someFieldsFound; }
    void setSomeFieldsFound(bool value) { m_someFieldsFound = value; }

    bool serializeMapToObject() const { return m_serializeMapToObject; }
    void setSerializeMapToObject(bool value) { m_serializeMapToObject = value; }

    bool isMapKeyDeserializationMode() const { return m_isMapKeyDeserializationMode; }
    void setIsMapKeyDeserializationMode(bool value) { m_isMapKeyDeserializationMode = value; }

    bool areStringConversionsAllowed() const { return m_allowStringConversions; }
    void setAllowStringConversions(bool value) { m_allowStringConversions = value; }

    template<typename T>
    bool isOptionalDefaultSerialization() const
    {
        auto type = std::type_index(typeid(T));
        return m_optionalDefaultSerialization && m_typeRecursions.find(type) == m_typeRecursions.end();
    }

    template<typename T>
    void addTypeToProcessed()
    {
        auto type = std::type_index(typeid(T));
        m_typeRecursions.insert(type);
    }

    template<typename T>
    void removeTypeFromProcessed()
    {
        auto type = std::type_index(typeid(T));
        m_typeRecursions.extract(type);
    }

    void setOptionalDefaultSerialization(bool value)
    {
        m_optionalDefaultSerialization = value;
    }

    bool doesDeserializeReplaceExistingOptional() const
    {
        return m_deserializeReplacesExistingOptional;
    }

    void deserializeReplacesExistingOptional(bool value)
    {
        m_deserializeReplacesExistingOptional = value;
    }

    bool isStrictMode() const { return m_strictMode; }
    void setStrictMode(bool value) { m_strictMode = value; }

    bool isChronoSerializedAsDouble() const { return m_chronoSerializedAsDouble; }
    void setChronoSerializedAsDouble(bool value) { m_chronoSerializedAsDouble = value; }

    const std::pair<QString, QString>& getFailedKeyValue() const { return m_failed; }
    void setFailedKeyValue(const std::pair<QString, QString>& failed)
    {
        if (m_failed.first.isEmpty())
            m_failed = failed;
        else
            m_failed.first.prepend(failed.first + '.');
    }

private:
    bool m_someFieldsNotFound{false};
    bool m_someFieldsFound{false};
    bool m_serializeMapToObject{false};
    bool m_isMapKeyDeserializationMode{false};
    bool m_allowStringConversions{false};
    bool m_optionalDefaultSerialization = false;
    bool m_strictMode = false;
    bool m_deserializeReplacesExistingOptional = true;

    // TODO: Make `m_chronoSerializedAsDouble = true` after version 4.0 support is dropped.
    bool m_chronoSerializedAsDouble = false;

    std::pair<QString, QString> m_failed;
    mutable std::unordered_set<std::type_index> m_typeRecursions;
};

class QnJsonSerializer:
    public QnContextSerializer<QJsonValue, QnJsonContext>,
    public QnStaticSerializerStorage<QnJsonSerializer, QJsonDetail::StorageInstance>
{
    typedef QnContextSerializer<QJsonValue, QnJsonContext> base_type;
public:
    QnJsonSerializer(QMetaType type): base_type(type) {}
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

template<class T>
void serialize(
    QnJsonContext* ctx,
    const std::optional<T>& value,
    const QString& key,
    QJsonObject* outTarget)
{
    NX_ASSERT(outTarget);

    QJsonValue jsonValue(QJsonValue::Undefined);
    QJson::serialize(ctx, value, &jsonValue);
    if (!jsonValue.isUndefined())
        (*outTarget)[key] = jsonValue;
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
        value, key, deprecatedFieldNames, structTypeInfo, optional);
    if (pos == value.end())
    {
        if (outFound != nullptr)
            *outFound = false;
        return optional;
    }

    if (outFound != nullptr)
        *outFound = true;

    if (QJson::deserialize(ctx, *pos, outTarget))
        return true;

    const auto failed = std::pair<QString, QString>(key, QJson::serialized(pos.value()));
    NX_WARNING(
        NX_SCOPE_TAG, "Can't deserialize field `%1` from value `%2`", failed.first, failed.second);
    ctx->setFailedKeyValue(failed);
    return optional && !ctx->isStrictMode();
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

    bool hasDirectSerializer = HasDirectObjectKeySerializer<T>::value;
    if (hasDirectSerializer && ctx->isMapKeyDeserializationMode())
    {
        jsonValue = QString::fromUtf8(value);
    }
    else if (QString errorMessage;
        !QJsonDetail::deserialize_json(value, &jsonValue, &errorMessage))
    {
        ctx->setFailedKeyValue(std::make_pair(QString(), errorMessage));
        return false;
    }

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

template<class T>
bool deserialize(const nx::ConstBufferRefType& value, T* outTarget)
{
    QnJsonContext ctx;
    return deserialize(
        &ctx,
        QByteArray::fromRawData(value.data(), (int) value.size()),
        outTarget);
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


template<class T>
bool deserializeAllowingOmittedValues(const QJsonValue& value, T* target,
    std::optional<QJsonValue>* outIncompleteJsonValue)
{
    QnJsonContext ctx;
    const bool result = QJson::deserialize(&ctx, value, target);
    if (ctx.areSomeFieldsNotFound())
        *outIncompleteJsonValue = value;
    else
        *outIncompleteJsonValue = std::nullopt;
    return result;
}

/**
 * Deserialize a value from a JSON utf-8-encoded string, allowing for omitted values.
 * @param value JSON string to deserialize.
 * @param outIncompleteJsonValue Returned when some values were omitted.
 * @return Whether no deserialization errors occurred, except for possible omitted values.
 */
template<class T>
bool deserializeAllowingOmittedValues(const QByteArray& value, T* target,
    std::optional<QJsonValue>* outIncompleteJsonValue)
{
    QJsonValue jsonValue;
    if (!QJsonDetail::deserialize_json(value, &jsonValue))
        return false;
    return deserializeAllowingOmittedValues(jsonValue, target, outIncompleteJsonValue);
}

class InvalidParameterException: public nx::utils::Exception
{
public:
    InvalidParameterException(const std::pair<QString, QString>& failedKeyValue):
        m_param(failedKeyValue.first), m_value(failedKeyValue.second)
    {
    }

    virtual QString message() const override
    {
        if (!m_param.isEmpty())
            return NX_FMT("Invalid parameter '%1': %2", m_param, m_value);

        // TODO: This case should be handled by the different exception type.
        return NX_FMT("Invalid value type%1%2", m_value.isEmpty() ? "" : ": ", m_value);
    }

    const QString& param() const { return m_param; }
    const QString& value() const { return m_value; }

private:
    const QString m_param;
    const QString m_value;
};

class InvalidJsonException: public nx::utils::Exception
{
public:
    InvalidJsonException(const QString& message): m_message(message) {}
    virtual QString message() const override { return m_message; }

private:
    const QString m_message;
};

template<class T>
T deserializeOrThrow(QnJsonContext* ctx, const QJsonValue& value)
{
    T result;
    if (QJson::deserialize(ctx, value, &result))
        return result;
    throw QJson::InvalidParameterException(ctx->getFailedKeyValue());
}

template<class T>
T deserializeOrThrow(
    const QJsonValue& value, bool allowStringConversions = false)
{
    if (value.isUndefined())
        throw QJson::InvalidJsonException("No JSON provided.");
    QnJsonContext ctx;
    ctx.setStrictMode(true);
    ctx.setAllowStringConversions(allowStringConversions);
    return QJson::deserializeOrThrow<T>(&ctx, value);
}

template<class T>
T deserializeOrThrow(const QByteArray& value)
{
    QJsonValue jsonValue;
    QString error;
    if (!QJsonDetail::deserialize_json(value, &jsonValue, &error))
        throw QJson::InvalidJsonException(error);
    QnJsonContext ctx;
    ctx.setStrictMode(true);
    return QJson::deserializeOrThrow<T>(&ctx, jsonValue);
}

template<class T>
T deserializeOrThrow(const nx::ConstBufferRefType& value)
{
    return deserializeOrThrow<T>(QByteArray::fromRawData(value.data(), value.size()));
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
T deserialized(const QByteArray& value, const T& defaultValue, bool* success)
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

template<class T>
T deserialized(const nx::ConstBufferRefType& value, const T& defaultValue, bool* success)
{
    return deserialized(
        QByteArray::fromRawData(value.data(), (int) value.size()),
        defaultValue,
        success);
}

template<class T>
T deserialized(const char* value, const T& defaultValue, bool* success)
{
    return deserialized(
        QByteArray::fromRawData(value, (int) std::strlen(value)),
        defaultValue,
        success);
}

NX_FUSION_API QString toString(QJsonValue::Type jsonType);

} // namespace QJson
