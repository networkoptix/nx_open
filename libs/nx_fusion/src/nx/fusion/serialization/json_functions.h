// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#ifndef Q_MOC_RUN
    #include <boost/preprocessor/tuple/enum.hpp>
#endif

#include <collection.h>

#include <QtCore/QBitArray>
#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QPair>
#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QSize>
#include <QtCore/QSizeF>
#include <QtCore/QUrl>
#include <QtCore/QtNumeric>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QRegion>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>

#include <nx/reflect/from_string.h>
#include <nx/reflect/to_string.h>
#include <nx/utils/buffer.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/uuid.h>

#include "collection_fwd.h"
#include "json.h"
#include "json_macros.h"
#include "lexical_functions.h"

QN_FUSION_DECLARE_FUNCTIONS(qint32, (json)) /* Needed for (de)serialize_numeric_enum below. */

inline void serialize(QnJsonContext *, const QJsonValue &value, QJsonValue *target) {
    *target = value;
}

inline bool deserialize(QnJsonContext *, const QJsonValue &value, QJsonValue *target) {
    *target = value;
    return true;
}

/* Situation with doubles is more complicated than with other built-in types
 * as we have to take non-finite numbers into account. */
inline void serialize(QnJsonContext *, const double &value, QJsonValue *target) {
    *target = QJsonValue(value);
}

inline bool deserialize(QnJsonContext *context, const QJsonValue &value, double *target) {
    if(value.type() == QJsonValue::Double) {
        *target = value.toDouble();
        return true;
    } else if(value.type() == QJsonValue::Null) {
        /* Strictly speaking, that's either a nan, or a +-inf,
         * but at this point we cannot say what it really was. */
        *target = qQNaN();
        return true;
    } else if(value.type() == QJsonValue::String && context->areStringConversionsAllowed()){
        bool isOk = false;
        *target = value.toString().toDouble(&isOk);
        return isOk;
    } else {
        return false;
    }
}

/* Floats are (de)serialized via conversion to/from double.
 * Note that we don't do any additional boundary checks for floats as we do for
 * integers as it doesn't really make much sense. */
inline void serialize(QnJsonContext *ctx, const float &value, QJsonValue *target) {
    ::serialize(ctx, static_cast<double>(value), target); /* Note the direct call instead of invocation through QJson. */
}

inline bool deserialize(QnJsonContext *ctx, const QJsonValue &value, float *target) {
    double tmp;
    if(!::deserialize(ctx, value, &tmp)) /* Note the direct call instead of invocation through QJson. */
        return false;

    *target = tmp;
    return true;
}

inline void serialize(QnJsonContext* ctx, const QVariant& value, QJsonValue* target)
{
    if (value == QVariant())
    {
        *target = QJsonValue();
        return;
    }

    auto serializer = QnJsonSerializer::serializer(value.userType());
    if (!NX_ASSERT(serializer,
        "Unregistered serializer for type %1 named \"%2\"", value.userType(), value.typeName()))
    {
        return;
    }

    serializer->serialize(ctx, value, target);
}

namespace QJsonDetail {

    template<class Element, class Tag>
    void serialize_collection_element(QnJsonContext *ctx, const Element &element, QJsonValue *target, const Tag &) {
        QJson::serialize(ctx, element, target);
    }

    template<class Element>
    void serialize_collection_element(QnJsonContext *ctx, const Element &element, QJsonValue *target, const QnCollection::map_tag &) {
        QJsonObject map;
        QJson::serialize(ctx, element.first, QLatin1String("key"), &map);
        QJson::serialize(ctx, element.second, QLatin1String("value"), &map);
        *target = map;
    }

    template<class Collection>
    void serialize_collection(QnJsonContext *ctx, const Collection &value, QJsonValue *target) {
        QJsonArray result;

        for(auto pos = boost::begin(value); pos != boost::end(value); ++pos) {
            QJsonValue element;
            serialize_collection_element(ctx, *pos, &element, typename QnCollection::collection_category<Collection>::type());
            result.push_back(element);
        }

        if (result.isEmpty() && ctx->isOptionalDefaultSerialization<Collection>())
        {
            using Item = std::decay_t<decltype(*boost::begin(value))>;
            using Tag = typename QnCollection::collection_category<Collection>::type;
            ctx->addTypeToProcessed<Collection>();
            if constexpr (std::is_same_v<Tag, QnCollection::map_tag>)
            {
                if (ctx->isOptionalDefaultSerialization<std::decay_t<typename Item::first_type>>()
                    || ctx->isOptionalDefaultSerialization<std::decay_t<typename Item::second_type>>())
                {
                    QJsonObject map;
                    ctx->addTypeToProcessed<std::decay_t<typename Item::first_type>()>();
                    QJson::serialize(ctx, std::decay_t<typename Item::first_type>(), "key", &map);
                    ctx->removeTypeFromProcessed<std::decay_t<typename Item::first_type>()>();
                    ctx->addTypeToProcessed<std::decay_t<typename Item::second_type>()>();
                    QJson::serialize(ctx, std::decay_t<typename Item::second_type>(), "value", &map);
                    ctx->removeTypeFromProcessed<std::decay_t<typename Item::second_type>()>();
                    result.push_back(map);
                }
            }
            else if (ctx->isOptionalDefaultSerialization<Item>())
            {
                QJsonValue item;
                ctx->addTypeToProcessed<Item>();
                QJson::serialize(ctx, Item(), &item);
                ctx->removeTypeFromProcessed<Item>();
                result.push_back(item);
            }
            ctx->removeTypeFromProcessed<Collection>();
        }

        *target = result;
    }

    inline QString convertQJsonValueToString(const QJsonValue& jsonValue)
    {
        if (jsonValue.type() == QJsonValue::String)
            return jsonValue.toString();

        return QString::fromUtf8(QJson::serialized(jsonValue));
    }

    template<typename Map>
    void serialize_generic_map_to_object(QnJsonContext* ctx, const Map& map, QJsonValue* target)
    {
        static_assert(
            std::is_same_v<
                typename QnCollection::collection_category<Map>::type,
                typename QnCollection::map_tag>);

        QJsonObject result;

        for (const auto& [key, value]: map)
        {
            QJsonValue serializedValue;
            QJson::serialize(ctx, value, &serializedValue);

            QJsonValue serializedKey;
            QJson::serialize(ctx, key, &serializedKey);

            result.insert(convertQJsonValueToString(serializedKey), serializedValue);
        }

        *target = result;
    }

    template<typename Map>
    void serialize_generic_map(QnJsonContext *ctx, const Map &value, QJsonValue *target)
    {
        if (ctx->serializeMapToObject())
            serialize_generic_map_to_object(ctx, value, target);
        else
            serialize_collection(ctx, value, target);

    }

    template<class Collection, class Element>
    bool deserialize_collection_element(QnJsonContext *ctx, const QJsonValue &value, Collection *target, const Element *, const QnCollection::list_tag &) {
        return QJson::deserialize(ctx, value, &*QnCollection::insert(*target, boost::end(*target), Element()));
    }

    template<class Collection, class Element>
    bool deserialize_collection_element(QnJsonContext *ctx, const QJsonValue &value, Collection *target, const Element *, const QnCollection::set_tag &) {
        Element element;
        if(!QJson::deserialize(ctx, value, &element))
            return false;

        QnCollection::insert(*target, boost::end(*target), std::move(element));
        return true;
    }

    template<class Collection, class Element>
    bool deserialize_collection_element(QnJsonContext *ctx, const QJsonValue &value, Collection *target, const Element *, const QnCollection::map_tag &) {
        if(value.type() != QJsonValue::Object)
            return false;
        QJsonObject element = value.toObject();

        typename Collection::key_type key;
        if(!QJson::deserialize(ctx, element, QLatin1String("key"), &key))
            return false;

        if(!QJson::deserialize(ctx, element, QLatin1String("value"), &(*target)[key]))
            return false;

        return true;
    }

    template<class Collection>
    bool deserialize_collection(QnJsonContext *ctx, const QJsonValue &value, Collection *target) {
        typedef typename std::iterator_traits<typename boost::range_mutable_iterator<Collection>::type>::value_type value_type;

        if(value.type() != QJsonValue::Array)
            return false;
        QJsonArray array = value.toArray();

        QnCollection::clear(*target);
        QnCollection::reserve(*target, array.size());

        for(auto pos = array.begin(); pos != array.end(); pos++)
            if(!deserialize_collection_element(ctx, *pos, target, static_cast<const value_type *>(NULL), typename QnCollection::collection_category<Collection>::type()))
                return false;

        return true;
    }

    template<typename Map>
    bool deserialize_generic_map_from_object(
        QnJsonContext* ctx,
        const QJsonObject& value,
        Map* target)
    {
        QnCollection::clear(*target);
        QnCollection::reserve(*target, value.size());

        for (auto it = value.begin(); it != value.end(); ++it)
        {
            typename Map::key_type deserializedKey;

            {
                nx::utils::ScopeGuard ctxGuard(
                    [ctx]() { ctx->setIsMapKeyDeserializationMode(false); });
                ctx->setIsMapKeyDeserializationMode(true);
                if (!QJson::deserialize(ctx, it.key().toUtf8(), &deserializedKey))
                    return false;
            }

            if (!QJson::deserialize(ctx, it.value(), &(*target)[deserializedKey]))
                return false;
        }

        return true;
    }

    template<typename Map>
    bool deserialize_generic_map(QnJsonContext* ctx, const QJsonValue& value, Map* target)
    {
        static_assert(
            std::is_same_v<
                typename QnCollection::collection_category<Map>::type,
                typename QnCollection::map_tag>);

        switch (value.type())
        {
            case QJsonValue::Object:
                return deserialize_generic_map_from_object(ctx, value.toObject(), target);
            case QJsonValue::Array:
                return deserialize_collection(ctx, value, target);
            default:
                return false;
        }
    }

    template<typename String>
    String fromQString(const QString& str)
    {
        if constexpr (std::is_same_v<String, std::string>)
            return str.toStdString();
        else
            return str;
    }

    template<typename String>
    QString toQString(const String& str)
    {
        if constexpr (std::is_same_v<std::decay_t<String>, std::string>)
            return QString::fromStdString(str);
        else
            return str;
    }

    template<class Map>
    void serialize_string_map(QnJsonContext *ctx, const Map &value, QJsonValue *target) {
        QJsonObject result;

        ctx->addTypeToProcessed<Map>();
        for(auto pos = boost::begin(value); pos != boost::end(value); pos++) {
            QJsonValue jsonValue;
            QJson::serialize(ctx, pos->second, &jsonValue);
            result.insert(toQString(pos->first), jsonValue);
        }
        ctx->removeTypeFromProcessed<Map>();

        using Item = std::decay_t<decltype(*boost::begin(value))>;
        if (result.isEmpty() && ctx->isOptionalDefaultSerialization<Item>())
        {
            QJsonValue jsonValue;
            ctx->addTypeToProcessed<Item>();
            QJson::serialize(ctx, std::decay_t<typename Item::second_type>(), &jsonValue);
            ctx->removeTypeFromProcessed<Item>();
            result.insert(toQString(std::decay_t<typename Item::first_type>()), jsonValue);
        }
        *target = result;
    }

    template<class Map>
    bool deserialize_string_map(QnJsonContext *ctx, const QJsonValue &value, Map *target) {
        if(value.type() != QJsonValue::Object)
            return false;
        QJsonObject map = value.toObject();

        QnCollection::clear(*target);
        QnCollection::reserve(*target, map.size());

        for(auto pos = map.begin(); pos != map.end(); pos++)
            if(!QJson::deserialize(ctx, pos.value(), &(*target)[fromQString<typename Map::key_type>(pos.key())]))
                return false;

        return true;
    }

    template<class T>
    void serialize_integer(QnJsonContext *ctx, const T &value, QJsonValue *target) {
        ::serialize(ctx, static_cast<double>(value), target); /* Note the direct call instead of invocation through QJson. */
    }

    template<class T>
    bool deserialize_integer(QnJsonContext *ctx, const QJsonValue &value, T *target) {
        double tmp;
        if(!::deserialize(ctx, value, &tmp)) /* Note the direct call instead of invocation through QJson. */
            return false;
        if(tmp < static_cast<double>(std::numeric_limits<T>::min()) || tmp > static_cast<double>(std::numeric_limits<T>::max()))
            return false;

        *target = static_cast<T>(tmp);
        return true;
    }

    template<class T>
    void serialize_integer_string(QnJsonContext *, const T &value, QJsonValue *target) {
        *target = QJsonValue(QnLexical::serialized(value));
    }

    template<class T>
    bool deserialize_integer_string(QnJsonContext *ctx, const QJsonValue &value, T *target) {
        /* Support both strings and doubles during deserialization, just to feel safe. */
        if(value.type() == QJsonValue::Double) {
            return QJsonDetail::deserialize_integer<T>(ctx, value, target);
        } else if(value.type() == QJsonValue::String) {
            return QnLexical::deserialize(value.toString(), target);
        } else {
            return false;
        }
    }

    template<typename T, typename... Args>
    inline bool deserializeVariantType(
        QnJsonContext* ctx,
        const QJsonValue& value,
        std::variant<Args...>* target)
    {
        // Note: Optional in variant is not supported. So we need temporary
        // setSomeFieldsNotFound(false) to check that variant field is not found.
        const bool areSomeFieldsNotFound = ctx->areSomeFieldsNotFound();
        nx::utils::ScopeGuard guard(
            [ctx, areSomeFieldsNotFound]()
            {
                if (areSomeFieldsNotFound)
                    ctx->setSomeFieldsNotFound(true);
            });
        if (areSomeFieldsNotFound)
            ctx->setSomeFieldsNotFound(false);

        T typedTarget;
        if (!deserialize(ctx, value, &typedTarget))
            return false;

        if (ctx->areSomeFieldsNotFound())
            return false;

        *target = typedTarget;
        return true;
    }

} // namespace QJsonDetail

/* Free-standing (de)serialization functions are picked up via ADL by
 * the actual implementation. Feel free to add them for your own types. */

#ifndef Q_MOC_RUN
#define QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS(TYPE, JSON_TYPE, JSON_GETTER)  \
inline void serialize(QnJsonContext *, const TYPE &value, QJsonValue *target) { \
    *target = QJsonValue(value);                                                \
}                                                                               \
                                                                                \
inline bool deserialize(QnJsonContext *, const QJsonValue &value, TYPE *target) { \
    if(value.type() != QJsonValue::JSON_TYPE)                                   \
        return false;                                                           \
                                                                                \
    *target = value.JSON_GETTER();                                              \
    return true;                                                                \
}

QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS(QString,      String, toString)
QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS(QJsonArray,   Array,  toArray)
QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS(QJsonObject,  Object, toObject)
#undef QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS

inline void serialize(QnJsonContext *, const bool &value, QJsonValue *target) {
    *target = QJsonValue(value);
}

inline bool deserialize(QnJsonContext *context, const QJsonValue &value, bool *target) {
    if(value.type() == QJsonValue::Bool) {
        *target = value.toBool();
        return true;
    }

    if (value.type() == QJsonValue::String && context->areStringConversionsAllowed()) {
        const auto stringValue = value.toString();
        for (const auto& key: {QStringLiteral("1"), QStringLiteral("true"), QStringLiteral("on")}) {
            if (QString::compare(stringValue, key, Qt::CaseInsensitive) == 0) {
                *target = true;
                return true;
            }
        }

        for (const auto& key: {QStringLiteral("0"), QStringLiteral("false"), QStringLiteral("off")}) {
            if (QString::compare(stringValue, key, Qt::CaseInsensitive) == 0) {
                *target = false;
                return true;
            }
        }
    }

    return false;
}

inline void serialize(QnJsonContext *, const std::string &value, QJsonValue *target) {
    *target = QJsonValue(QString::fromStdString(value));
}

inline bool deserialize(QnJsonContext *, const QJsonValue &value, std::string *target) {
    if(value.type() != QJsonValue::String)
        return false;

    *target = value.toString().toStdString();
    return true;
}

#define QN_DEFINE_JSON_CHRONO_SERIALIZATION_FUNCTIONS(TYPE) \
    inline void serialize( \
        QnJsonContext* jsonContext, const std::chrono::TYPE& value, QJsonValue* target) \
    { \
        *target = jsonContext->isChronoSerializedAsDouble() \
            ? QJsonValue((double) value.count()) \
            : QJsonValue(QString::number(value.count())); \
    } \
    \
    inline bool deserialize(QnJsonContext*, const QJsonValue& value, std::chrono::TYPE* target) \
    { \
        if (value.type() != QJsonValue::String && value.type() != QJsonValue::Double) \
            return false; \
        *target = std::chrono::TYPE(value.toVariant().value<std::chrono::TYPE::rep>()); \
        return true; \
    }

QN_DEFINE_JSON_CHRONO_SERIALIZATION_FUNCTIONS(seconds)
QN_DEFINE_JSON_CHRONO_SERIALIZATION_FUNCTIONS(milliseconds)
QN_DEFINE_JSON_CHRONO_SERIALIZATION_FUNCTIONS(microseconds)

#undef QN_DEFINE_JSON_CHRONO_SERIALIZATION_FUNCTIONS

/**
 * Representing system_clock::time_point in json as milliseconds since epoch (1970-01-01T00:00).
 */
inline void serialize(
    QnJsonContext* jsonContext,
    const std::chrono::system_clock::time_point& value,
    QJsonValue* target)
{
    using namespace std::chrono;

    const auto millisSinceEpoch =
        duration_cast<milliseconds>(value.time_since_epoch()).count();
    *target = jsonContext->isChronoSerializedAsDouble()
        ? QJsonValue((double) millisSinceEpoch)
        : QJsonValue(QString::number(millisSinceEpoch));
}

inline bool deserialize(
    QnJsonContext*,
    const QJsonValue& value,
    std::chrono::system_clock::time_point* target)
{
    if (value.type() != QJsonValue::String && value.type() != QJsonValue::Double)
        return false;

    const auto millisSinceEpoch = value.toVariant().toULongLong();
    // Initializing with epoch.
    *target = std::chrono::system_clock::time_point();
    // Adding milliseconds since epoch.
    *target += std::chrono::milliseconds(millisSinceEpoch);
    return true;
}

template<typename T>
inline void serialize(
    QnJsonContext* ctx,
    const std::optional<T>& value,
    QJsonValue* target)
{
    if (!value)
    {
        *target = QJsonValue(QJsonValue::Undefined);
        return;
    }

    QJson::serialize<T>(ctx, *value, target);
}

template<typename T>
inline bool deserialize(
    QnJsonContext* ctx,
    const QJsonValue& value,
    std::optional<T>* target)
{
    if (!ctx->doesDeserializeReplaceExistingOptional() && *target)
        return true;

    if (value.isUndefined())
    {
        target->reset();
        return true;
    }

    *target = T();
    return QJson::deserialize<T>(ctx, value, &**target);
}

inline void serialize(QnJsonContext* /*ctx*/, const std::nullptr_t& /*value*/, QJsonValue* target)
{
    *target = QJsonValue();
}

inline bool deserialize(QnJsonContext* /*ctx*/, const QJsonValue& value, std::nullptr_t* target)
{
    *target = nullptr;
    return value.isNull();
}

template<typename... Args>
inline void serialize(QnJsonContext* ctx, const std::variant<Args...>& value, QJsonValue* target)
{
    if (ctx->isOptionalDefaultSerialization<void>())
    {
        *target = QJsonValue();
        return;
    }
    std::visit(
        [ctx, target](auto&& arg) { QJson::serialize(ctx, std::move(arg), target); }, value);
}

template<typename... Args>
inline bool deserialize(QnJsonContext* ctx, const QJsonValue& value, std::variant<Args...>* target)
{
    return (... || QJsonDetail::deserializeVariantType<Args>(ctx, value, target));
}

#define QN_DEFINE_INTEGER_JSON_SERIALIZATION_FUNCTIONS(TYPE)                    \
inline void serialize(QnJsonContext *ctx, const TYPE &value, QJsonValue *target) { \
    QJsonDetail::serialize_integer<TYPE>(ctx, value, target);                   \
}                                                                               \
                                                                                \
inline bool deserialize(QnJsonContext *ctx, const QJsonValue &value, TYPE *target) { \
    return QJsonDetail::deserialize_integer<TYPE>(ctx, value, target);          \
}

QN_DEFINE_INTEGER_JSON_SERIALIZATION_FUNCTIONS(char);
QN_DEFINE_INTEGER_JSON_SERIALIZATION_FUNCTIONS(signed char); /* char, signed char and unsigned char are distinct types. */
QN_DEFINE_INTEGER_JSON_SERIALIZATION_FUNCTIONS(unsigned char);
QN_DEFINE_INTEGER_JSON_SERIALIZATION_FUNCTIONS(short);
QN_DEFINE_INTEGER_JSON_SERIALIZATION_FUNCTIONS(unsigned short);
QN_DEFINE_INTEGER_JSON_SERIALIZATION_FUNCTIONS(int);
QN_DEFINE_INTEGER_JSON_SERIALIZATION_FUNCTIONS(unsigned int);
#undef QN_DEFINE_INTEGER_JSON_SERIALIZATION_FUNCTIONS

#define QN_DEFINE_INTEGER_STRING_JSON_SERIALIZATION_FUNCTIONS(TYPE)             \
inline void serialize(QnJsonContext *ctx, const TYPE &value, QJsonValue *target) { \
    QJsonDetail::serialize_integer_string<TYPE>(ctx, value, target);            \
}                                                                               \
                                                                                \
inline bool deserialize(QnJsonContext *ctx, const QJsonValue &value, TYPE *target) { \
    return QJsonDetail::deserialize_integer_string<TYPE>(ctx, value, target);   \
}

QN_DEFINE_INTEGER_STRING_JSON_SERIALIZATION_FUNCTIONS(long)
QN_DEFINE_INTEGER_STRING_JSON_SERIALIZATION_FUNCTIONS(unsigned long)
QN_DEFINE_INTEGER_STRING_JSON_SERIALIZATION_FUNCTIONS(long long)
QN_DEFINE_INTEGER_STRING_JSON_SERIALIZATION_FUNCTIONS(unsigned long long)
#undef QN_DEFINE_INTEGER_STRING_JSON_SERIALIZATION_FUNCTIONS

template<typename A, typename B>
inline void serialize(
    QnJsonContext* ctx,
    const QPair<A, B>& value,
    QJsonValue* target)
{
    *target = QJsonArray{{}, {}};
    QJson::serialize<A>(ctx, value.first, &target[0]);
    QJson::serialize<B>(ctx, value.second, &target[1]);
}

template<typename A, typename B>
inline bool deserialize(
    QnJsonContext* ctx,
    const QJsonValue& value,
    QPair<A, B>* target)
{
    if (!value.isArray())
        return false;

    const auto array = value.toArray();
    if (array.size() != 2)
        return false;

    *target = QPair<A, B>();
    return QJson::deserialize<A>(ctx, array[0], &target->first)
        && QJson::deserialize<B>(ctx, array[1], &target->second);
}

#define QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(TYPE, TPL_DEF, TPL_ARG, IMPL) \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF)>                                          \
void serialize(QnJsonContext *ctx, const TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> &value, QJsonValue *target) { \
    QJsonDetail::BOOST_PP_CAT(serialize_, IMPL)(ctx, value, target);            \
}                                                                               \
                                                                                \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF)>                                          \
bool deserialize(QnJsonContext *ctx, const QJsonValue &value, TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> *target) { \
    return QJsonDetail::BOOST_PP_CAT(deserialize_, IMPL)(ctx, value, target);   \
}                                                                               \

QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(QSet, (class T), (T), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(QList, (class T), (T), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(QLinkedList, (class T), (T), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(QVarLengthArray, (class T, qsizetype N), (T, N), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(QMap, (class Key, class T), (Key, T), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(QHash, (class Key, class T), (Key, T), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(std::vector, (class T, class Allocator), (T, Allocator), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(std::list, (class T, class Allocator), (T, Allocator), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(std::set, (class Key, class Predicate, class Allocator), (Key, Predicate, Allocator), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(std::map, (class Key, class T, class Predicate, class Allocator), (Key, T, Predicate, Allocator), generic_map);

QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(QMap, (class T), (QString, T), string_map);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(QHash, (class T), (QString, T), string_map);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(std::map, (class T, class Predicate, class Allocator), (QString, T, Predicate, Allocator), string_map);

QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(
    std::map,
    (class T, class Predicate, class Allocator),
    (std::string, T, Predicate, Allocator),
    string_map);

#undef QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS
#endif // Q_MOC_RUN

template<typename T, size_t N>
inline void serialize(QnJsonContext* ctx, const std::array<T, N>& value, QJsonValue* target)
{
    QJsonDetail::serialize_collection(ctx, value, target);
}

template<typename T, size_t N>
inline bool deserialize(QnJsonContext* ctx, const QJsonValue& value, std::array<T, N>* target)
{
    std::vector<T> vector;
    if (!QJsonDetail::deserialize_collection(ctx, value, &vector) || vector.size() != N)
        return false;

    auto targetIt = target->begin();
    for (auto& vectorIt: vector)
        *(targetIt++) = std::move(vectorIt);

    return true;
}

template<typename T>
void serialize(QnJsonContext* ctx,
    const T& value,
    QJsonValue* target,
    typename std::enable_if<QnSerialization::IsEnumOrFlags<T>::value>::type* = nullptr)
{
    if constexpr (QnSerialization::IsInstrumentedEnumOrFlags<T>::value)
    {
        const std::string tmp = nx::reflect::toString(value);
        // Note the direct call instead of invocation through QJson.
        ::serialize(ctx, tmp, target);
    }
    else
    {
        QString tmp;
        QnLexical::serialize(value, &tmp);
        // Note the direct call instead of invocation through QJson.
        ::serialize(ctx, tmp, target);
    }
}

template<typename T>
bool deserialize(QnJsonContext* ctx,
    const QJsonValue& value,
    T* target,
    typename std::enable_if<QnSerialization::IsEnumOrFlags<T>::value>::type* = nullptr)
{
    if (value.type() == QJsonValue::String)
    {
        if constexpr (QnSerialization::IsInstrumentedEnumOrFlags<T>::value)
            return nx::reflect::fromString(value.toString().toStdString(), target);
        else
            return QnLexical::deserialize(value.toString(), target);
    }

    if (value.type() == QJsonValue::Double)
    {
        qint32 tmp;
        // Note the direct call instead of invocation through QJson.
        if (::deserialize(ctx, value, &tmp))
        {
            *target = static_cast<T>(tmp);
            return true;
        }
    }

    return false;
}

QN_FUSION_DECLARE_FUNCTIONS(QByteArray, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::Buffer, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QnLatin1Array, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QBitArray, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QColor, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QBrush, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QSize, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QSizeF, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QRect, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QRectF, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QPointF, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QPoint, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QRegion, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QVector2D, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QVector3D, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QVector4D, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::Uuid, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QUrl, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QFont, (json), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QDateTime, (json), NX_FUSION_API)

#define QN_FUSION_REGISTER_DIRECT_OBJECT_KEY_SERIALIZER(TYPE)\
template<> \
struct HasDirectObjectKeySerializer<TYPE>: public std::true_type \
{ \
};

QN_FUSION_REGISTER_DIRECT_OBJECT_KEY_SERIALIZER(nx::Uuid)

void qnJsonFunctionsUnitTest();
