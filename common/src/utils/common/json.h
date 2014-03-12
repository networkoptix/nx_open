#ifndef QN_JSON_H
#define QN_JSON_H

#include <cassert>
#include <limits>

#ifndef Q_MOC_RUN
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/transform.hpp>
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/remove_reference.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/mpl/bool.hpp>
#endif

#include <QtCore/QJsonValue>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include "adl_wrapper.h"
#include "unused.h"
#include "json_fwd.h"
#include "json_context.h"
#include "id.h"

namespace QJsonDetail {
    void serialize_json(const QJsonValue &value, QByteArray *target, QJsonDocument::JsonFormat format = QJsonDocument::Compact);
    bool deserialize_json(const QByteArray &value, QJsonValue *target);

    template<class T>
    void serialize_value_direct(QnJsonContext *ctx, const T &value, QJsonValue *target) {
        serialize(ctx, value, target); /* That's the place where ADL kicks in. */
    }

    template<class T>
    bool deserialize_value_direct(QnJsonContext *ctx, const QJsonValue &value, T *target) {
        /* That's the place where ADL kicks in.
         * 
         * Note that we wrap a json value into a wrapper so that
         * ADL would find only overloads with QJsonValue as the first parameter. 
         * Otherwise other overloads could be discovered. */
        return deserialize(ctx, adlWrap(value), target);
    }

    // TODO: #Elric qMetaTypeId uses atomics for custom types. Maybe introduce local cache?

    template<class T>
    struct is_metatype_defined: boost::mpl::bool_<QMetaTypeId2<T>::Defined> {};

    template<class T>
    void serialize_value(QnJsonContext *ctx, const T &value, QJsonValue *target, typename boost::enable_if<is_metatype_defined<T> >::type * = NULL) {
        QnJsonSerializer *serializer = QJsonDetail::ContextAccess::serializer(ctx, qMetaTypeId<T>());
        if(serializer) {
            serializer->serialize(ctx, static_cast<const void *>(&value), target);
        } else {
            serialize_value_direct(ctx, value, target);
        }
    }

    template<class T>
    void serialize_value(QnJsonContext *ctx, const T &value, QJsonValue *target, typename boost::disable_if<is_metatype_defined<T> >::type * = NULL) {
        serialize_value_direct(ctx, value, target);
    }

    template<class T>
    bool deserialize_value(QnJsonContext *ctx, const QJsonValue &value, T *target, typename boost::enable_if<is_metatype_defined<T> >::type * = NULL) {
        QnJsonSerializer *serializer = QJsonDetail::ContextAccess::serializer(ctx, qMetaTypeId<T>());
        if(serializer) {
            return serializer->deserialize(ctx, value, static_cast<void *>(target));
        } else {
            return deserialize_value_direct(ctx, value, target);
        }
    }

    template<class T>
    bool deserialize_value(QnJsonContext *ctx, const QJsonValue &value, T *target, typename boost::disable_if<is_metatype_defined<T> >::type * = NULL) {
        return deserialize_value_direct(ctx, value, target);
    }

} // namespace QJsonDetail


namespace QJson {
    enum Option {
        Optional = 0x1
    };
    Q_DECLARE_FLAGS(Options, Option)

    /**
     * Serializes the given value into intermediate JSON representation.
     * 
     * \param ctx                       JSON context to use.
     * \param value                     Value to serialize.
     * \param[out] target               Target JSON value, must not be NULL.
     */
    template<class T>
    void serialize(QnJsonContext *ctx, const T &value, QJsonValue *target) {
        assert(target);
        QJsonDetail::serialize_value(ctx, value, target);
    }

    template<class T>
    void serialize(QnJsonContext *ctx, const T &value, QJsonValueRef *target) {
        assert(target);

        QJsonValue jsonValue;
        QJsonDetail::serialize_value(ctx, value, &jsonValue);
        *target = jsonValue;
    }

    template<class T>
    void serialize(QnJsonContext *ctx, const T &value, const QString &key, QJsonObject *target) {
        assert(target);

        QJsonValueRef jsonValue = (*target)[key];
        QJson::serialize(ctx, value, &jsonValue);
    }

    /**
     * Serializes the given value into a JSON string.
     * 
     * \param ctx                       JSON context to use.
     * \param value                     Value to serialize.
     * \param[out] target               Target JSON string, must not be NULL.
     */
    template<class T>
    void serialize(QnJsonContext *ctx, const T &value, QByteArray *target) {
        assert(target);

        QJsonValue jsonValue;
        QJson::serialize(ctx, value, &jsonValue);
        QJsonDetail::serialize_json(jsonValue, target);
    }

    template<class T>
    void serialize(const T &value, QJsonValue *target) {
        QnJsonContext ctx;
        QJson::serialize(&ctx, value, target);
    }

    template<class T>
    void serialize(const T &value, QJsonValueRef *target) {
        QnJsonContext ctx;
        QJson::serialize(&ctx, value, target);
    }

    template<class T>
    void serialize(const T &value, const QString &key, QJsonObject *target) {
        QnJsonContext ctx;
        QJson::serialize(&ctx, value, key, target);
    }

    template<class T>
    void serialize(const T &value, QByteArray *target) {
        QnJsonContext ctx;
        QJson::serialize(&ctx, value, target);
    }


    /**
     * Deserializes the given intermediate representation of a JSON object.
     * Note that <tt>boost::enable_if</tt> is used to prevent implicit conversions
     * in the first argument.
     * 
     * \param ctx                       JSON context to use.
     * \param value                     Intermediate JSON representation to deserialize.
     * \param[out] target               Deserialization target, must not be NULL.
     * \returns                         Whether the deserialization was successful.
     */
    template<class T, class QJsonValue>
    bool deserialize(QnJsonContext *ctx, const QJsonValue &value, T *target, typename boost::enable_if<boost::is_same<QJsonValue, ::QJsonValue> >::type * = NULL) {
        assert(target);

        return QJsonDetail::deserialize_value(ctx, value, target);
    }

    template<class T>
    bool deserialize(QnJsonContext *ctx, const QJsonValueRef &value, T *target) {
        assert(target);

        return QJsonDetail::deserialize_value(ctx, value, target);
    }

    template<class T>
    bool deserialize(QnJsonContext *ctx, const QJsonObject &value, const QString &key, T *target, bool optional = false, bool *found = NULL) {
        QJsonObject::const_iterator pos = value.find(key);
        if(pos == value.end()) {
            if(found)
                *found = false;
            return optional;
        } else {
            if(found)
                *found = true;
            return QJson::deserialize(ctx, *pos, target);
        }
    }

    /**
     * Deserializes a value from a JSON string.
     * 
     * \param ctx                       JSON context to use.
     * \param value                     JSON string to deserialize.
     * \param[out] target               Deserialization target, must not be NULL.
     * \returns                         Whether the deserialization was successful.
     */
    template<class T>
    bool deserialize(QnJsonContext *ctx, const QByteArray &value, T *target) {
        assert(target);

        QJsonValue jsonValue;
        if(!QJsonDetail::deserialize_json(value, &jsonValue))
            return false;
        return QJson::deserialize(ctx, jsonValue, target);
    }

    template<class T, class QJsonValue>
    bool deserialize(const QJsonValue &value, T *target, typename boost::enable_if<boost::is_same<QJsonValue, ::QJsonValue> >::type * = NULL) {
        QnJsonContext ctx;
        return QJson::deserialize(&ctx, value, target);
    }

    template<class T>
    bool deserialize(const QJsonValueRef &value, T *target) {
        QnJsonContext ctx;
        return QJson::deserialize(&ctx, value, target);
    }

    template<class T>
    bool deserialize(const QJsonObject &value, const QString &key, T *target, bool optional = false) {
        QnJsonContext ctx;
        return QJson::deserialize(&ctx, value, key, target, optional);
    }

    template<class T>
    bool deserialize(const QByteArray &value, T *target) {
        QnJsonContext ctx;
        return QJson::deserialize(&ctx, value, target);
    }


    /**
     * Serializes the given value into a JSON string and returns it in the utf-8 format.
     *
     * \param value                     Value to serialize.
     * \returns                         Result JSON data.
     */
    template<class T>
    QByteArray serialized(const T &value) {
        QByteArray result;
        QJson::serialize(value, &result);
        return result;
    }

    /**
     * Deserializes a value from a JSON utf-8-encoded string.
     *
     * \param value                     JSON string to deserialize.
     * \param defaultValue              Value to return in case of deserialization failure.
     * \param[out] success              Deserialization status.
     * \returns                         Deserialization target.
     */
    template<class T>
    T deserialized(const QByteArray &value, const T &defaultValue = T(), bool *success = NULL) {
        T target;
        bool result = QJson::deserialize(value, &target);
        if (success)
            *success = result;
        return result ? target : defaultValue;
    }

} // namespace QJson


namespace QJsonAccessors {
    template<class Class, class T>
    T getMember(const Class &object, T Class::*getter) {
        return object.*getter;
    }

    template<class Class, class T>
    T getMember(const Class &object, T (Class::*getter)() const) {
        return (object.*getter)();
    }

    template<class Class, class Getter>
    auto getMember(const Class &object, const Getter &getter) -> decltype(getter(object)) {
        return getter(object);
    }

    template<class Class, class T>
    void setMember(Class &object, T Class::*setter, const T &value) {
        object.*setter = value;
    }

    template<class Class, class R, class P, class T>
    void setMember(Class &object, R (Class::*setter)(P), const T &value) {
        (object.*setter)(value);
    }

    template<class Class, class Setter, class T>
    void setMember(Class &object, const Setter &setter, const T &value) {
        setter(object, value);
    }

} // namespace QJsonAccessors


namespace QJsonDetail {
    struct TrueChecker {
        template<class T>
        bool operator()(const T &) const { return true; }
    };

    template<class Class, class Getter>
    inline void serializeMember(QnJsonContext *ctx, const Class &object, const Getter &getter, const QString &key, QJsonObject *target, QJson::Options globalOptions, QJson::Options localOptions = 0) {
        using namespace QJsonAccessors;
        unused(globalOptions, localOptions);

        QJson::serialize(ctx, getMember(object, getter), key, target);
    }

    template<class Class, class Getter, class Checker>
    inline void serializeMember(QnJsonContext *ctx, const Class &object, const Getter &getter, const QString &key, QJsonObject *target, QJson::Options globalOptions, const Checker &checker, QJson::Options localOptions = 0) {
        using namespace QJsonAccessors;
        unused(globalOptions, localOptions);

        if(getMember(object, checker))
            QJson::serialize(ctx, getMember(object, getter), key, target);
    }

    template<class Class, class Setter, class T>
    inline bool deserializeMemberInternal(QnJsonContext *ctx, const QJsonObject &value, const QString &key, Class *object, const Setter &setter, QJson::Options options, const T *) {
        using namespace QJsonAccessors;

        bool found = false;
        T member;
        if(!QJson::deserialize(ctx, value, key, &member, options & QJson::Optional, &found))
            return false;
        if(found)
            setMember(*object, setter, member);
        return true;
    }

    template<class Class, class Setter, class T>
    inline bool deserializeMemberInternal(QnJsonContext *ctx, const QJsonObject &value, const QString &key, Class *object, T Class::*setter, QJson::Options options, const T *) {
        return QJson::deserialize(ctx, value, key, &object->*setter, options & QJson::Optional);
    }

    template<class Class, class Setter, class Getter>
    inline bool deserializeMember(QnJsonContext *ctx, const QJsonObject &value, const QString &key, Class *object, const Getter &getter, const Setter &setter, QJson::Options globalOptions, QJson::Options localOptions = 0) {
        return deserializeMember(ctx, value, key, object, getter, setter, globalOptions, TrueChecker(), localOptions);
    }

    template<class Class, class Setter, class Getter, class Checker>
    inline bool deserializeMember(QnJsonContext *ctx, const QJsonObject &value, const QString &key, Class *object, const Getter &getter, const Setter &setter, QJson::Options globalOptions, const Checker &checker, QJson::Options localOptions = 0) {
        using namespace QJsonAccessors;
        unused(getter, checker);

        typedef typename boost::remove_reference<decltype(getMember(*object, getter))>::type member_type;
        return deserializeMemberInternal(ctx, value, key, object, setter, globalOptions | localOptions, static_cast<const member_type *>(NULL));
    }


} // namespace QJsonDetail

#ifndef Q_MOC_RUN

/**
 * This macro generates the necessary boilerplate to (de)serialize struct types.
 * It uses field names for JSON keys.
 *
 * \param TYPE                          Struct type to define (de)serialization functions for.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be (de)serialized.
 * \param OPTIONS                       Additional (de)serialization options.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS_EX(TYPE, FIELD_SEQ, OPTIONS, ... /* PREFIX */) \
    QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS_EX(TYPE, BOOST_PP_SEQ_TRANSFORM(QN_CLASS_FROM_STRUCT_JSON_FIELD_I, TYPE, FIELD_SEQ), OPTIONS, ##__VA_ARGS__)

#define QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
    QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS_EX(TYPE, FIELD_SEQ, 0, ##__VA_ARGS__)

#define QN_CLASS_FROM_STRUCT_JSON_FIELD_I(R, TYPE, FIELD)                       \
    (&TYPE::FIELD, &TYPE::FIELD, BOOST_PP_STRINGIZE(FIELD))


/**
 * This macro generates the necessary boilerplate to (de)serialize class types.
 * 
 * \param TYPE                          Class type to define (de)serialization functions for.
 * \param FIELD_SEQ                     Preprocessor sequence of field descriptions for
 *                                      the given class type.
 * \param OPTIONS                       Additional (de)serialization options.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS_EX(TYPE, FIELD_SEQ, OPTIONS, ... /* PREFIX */) \
__VA_ARGS__ void serialize(QnJsonContext *ctx, const TYPE &value, QJsonValue *target) { \
    const QJson::Options options = OPTIONS;                                     \
    QJsonObject result;                                                         \
    BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_CLASS_JSON_SERIALIZATION_STEP_I, ~, FIELD_SEQ) \
    *target = result;                                                           \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(QnJsonContext *ctx, const QJsonValue &value, TYPE *target) { \
    if(value.type() != QJsonValue::Object)                                      \
        return false;                                                           \
    QJsonObject object = value.toObject();                                      \
                                                                                \
    const QJson::Options options = OPTIONS;                                     \
    TYPE result;                                                                \
    BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_CLASS_JSON_DESERIALIZATION_STEP_I, ~, FIELD_SEQ) \
    *target = result;                                                           \
    return true;                                                                \
}

#define QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
    QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS_EX(TYPE, FIELD_SEQ, 0, ##__VA_ARGS__)

#define QN_DEFINE_CLASS_JSON_SERIALIZATION_STEP_I(R, DATA, FIELD)               \
    QN_DEFINE_CLASS_JSON_SERIALIZATION_STEP_II FIELD

#define QN_DEFINE_CLASS_JSON_SERIALIZATION_STEP_II(GETTER, SETTER, NAME, ... /* OPTIONS */) \
    QJsonDetail::serializeMember(ctx, value, GETTER, QStringLiteral(NAME), &result, options, ##__VA_ARGS__);

#define QN_DEFINE_CLASS_JSON_DESERIALIZATION_STEP_I(R, DATA, FIELD)             \
    QN_DEFINE_CLASS_JSON_DESERIALIZATION_STEP_II FIELD

#define QN_DEFINE_CLASS_JSON_DESERIALIZATION_STEP_II(GETTER, SETTER, NAME, ... /* OPTIONS */) \
    if(!QJsonDetail::deserializeMember(ctx, object, QStringLiteral(NAME), &result, GETTER, SETTER, options, ##__VA_ARGS__)) \
        return false;


#define QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(TYPE, ... /* PREFIX */)  \
__VA_ARGS__ void serialize(QnJsonContext *, const TYPE &value, QJsonValue *target) { \
    *target = QnLexical::serialized(value);                                     \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(QnJsonContext *, const QJsonValue &value, TYPE *target) { \
    QString string;                                                             \
    return QJson::deserialize(value, &string) && QnLexical::deserialize(string, target); \
}


#define QN_DEFINE_ENUM_CAST_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(TYPE, ... /* PREFIX */) \
    QN_DEFINE_ENUM_CAST_LEXICAL_SERIALIZATION_FUNCTIONS(TYPE, ##__VA_ARGS__)    \
    QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(TYPE, ##__VA_ARGS__) // TODO: #Elric there is no support for Json int here!!!

#define QN_DEFINE_ENUM_MAPPED_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(TYPE, ... /* PREFIX */) \
    QN_DEFINE_ENUM_MAPPED_LEXICAL_SERIALIZATION_FUNCTIONS(TYPE, ##__VA_ARGS__)  \
    QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(TYPE, ##__VA_ARGS__)

#else // Q_MOC_RUN

/* Qt moc chokes on our macro hell, so we make things easier for it. */
#define QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS_EX(...)
#define QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(...)
#define QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS_EX(...)
#define QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(...)
#define QN_DEFINE_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(...)

#endif // Q_MOC_RUN

#include "json_functions.h"

#endif // QN_JSON_H
