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
#include <boost/utility/enable_if.hpp>
#endif

#include <QtCore/QJsonValue>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

namespace QJsonDetail {
    template<class T>
    class ValueWrapper {
    public:
        ValueWrapper(const T &value): m_value(value) {}

        operator const T &() const {
            return m_value;
        }

    private:
        const T &m_value;
    };

    void serialize_json(const QJsonValue &value, QByteArray *target, QJsonDocument::JsonFormat format = QJsonDocument::Compact);
    bool deserialize_json(const QByteArray &value, QJsonValue *target);

    template<class T>
    void serialize_value(const T &value, QJsonValue *target) {
        serialize(value, target); /* That's the place where ADL kicks in. */
    }

    template<class T>
    bool deserialize_value(const QJsonValue &value, T *target) {
        /* That's the place where ADL kicks in.
         * 
         * Note that we wrap a json value into a wrapper so that
         * ADL would find only overloads with QJsonValue as the first parameter. 
         * Otherwise other overloads could be discovered. */
        return deserialize(ValueWrapper<QJsonValue>(value), target); /* That's the place where ADL kicks in. */
    }

} // namespace QJsonDetail


namespace QJson {
    /**
     * Serializes the given value into intermediate JSON representation.
     * 
     * \param value                     Value to serialize.
     * \param[out] target               Target JSON value, must not be NULL.
     */
    template<class T>
    void serialize(const T &value, QJsonValue *target) {
        assert(target);

        QJsonDetail::serialize_value(value, target);
    }

    template<class T>
    void serialize(const T &value, QJsonValueRef *target) {
        assert(target);

        QJsonValue jsonValue;
        QJsonDetail::serialize_value(value, &jsonValue);
        *target = jsonValue;
    }

    template<class T>
    void serialize(const T &value, const QString &key, QJsonObject *target) {
        assert(target);

        QJsonValueRef jsonValue = (*target)[key];
        QJson::serialize(value, &jsonValue);
    }

    template<class T>
    void serialize(const T &value, const char *key, QJsonObject *target) {
        QJson::serialize(value, QLatin1String(key), target); // TODO: #Elric remove, use QStringLiteral
    }

    /**
     * Serializes the given value into a JSON string.
     * 
     * \param value                     Value to serialize.
     * \param[out] target               Target JSON string, must not be NULL.
     */
    template<class T>
    void serialize(const T &value, QByteArray *target) {
        assert(target);

        QJsonValue jsonValue;
        QJsonDetail::serialize_value(value, &jsonValue);
        QJsonDetail::serialize_json(jsonValue, target);
    }


    /**
     * Deserializes the given intermediate representation of a JSON object.
     * Note that <tt>boost::enable_if</tt> is used to prevent implicit conversions
     * in the first argument.
     * 
     * \param value                     Intermediate JSON representation to deserialize.
     * \param[out] target               Deserialization target, must not be NULL.
     * \returns                         Whether the deserialization was successful.
     */
    template<class T, class QJsonValue>
    bool deserialize(const QJsonValue &value, T *target, typename boost::enable_if<boost::is_same<QJsonValue, ::QJsonValue> >::type * = NULL) {
        assert(target);

        return QJsonDetail::deserialize_value(value, target);
    }

    template<class T>
    bool deserialize(const QJsonValueRef &value, T *target) {
        assert(target);

        return QJsonDetail::deserialize_value(value, target);
    }

    template<class T>
    bool deserialize(const QJsonObject &value, const QString &key, T *target, bool optional = false) {
        QJsonObject::const_iterator pos = value.find(key);
        if(pos == value.end()) {
            return optional;
        } else {
            return QJson::deserialize(*pos, target);
        }
    }

    template<class T>
    bool deserialize(const QJsonObject &value, const char *key, T *target, bool optional = false) {
        return QJson::deserialize(value, QLatin1String(key), target, optional); // TODO: #Elric remove, use QStringLiteral
    }

    /**
     * Deserializes a value from a JSON string.
     * 
     * \param value                     JSON string to deserialize.
     * \param[out] target               Deserialization target, must not be NULL.
     * \returns                         Whether the deserialization was successful.
     */
    template<class T>
    bool deserialize(const QByteArray &value, T *target) {
        assert(target);

        QJsonValue jsonValue;
        if(!QJsonDetail::deserialize_json(value, &jsonValue))
            return false;
        return QJsonDetail::deserialize_value(jsonValue, target);
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
     * \param[out] success              Deserialization status.
     * \returns                         Deserialization target.
     */
    template<class T>
    T deserialized(const QString &value, bool *success = NULL) {
        T target;
        bool result = QJson::deserialize(value.toUtf8(), &target);
        if (success)
            *success = result;
        return target;
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
    template<class Class, class Setter, class T>
    bool deserializeMember(const QJsonObject &value, const QString &key, Class &object, const Setter &setter, const T * = NULL) {
        using namespace QJsonAccessors;

        T member;
        if(!QJson::deserialize(value, key, &member))
            return false;
        setMember(object, setter, member);
        return true;
    }

    template<class Class, class Setter, class T>
    bool deserializeMember(const QJsonObject &value, const QString &key, Class &object, T Class::*setter, const T * = NULL) {
        return QJson::deserialize(value, key, &object.*setter);
    }

} // namespace QJsonDetail

#ifndef Q_MOC_RUN

/**
 * \param TYPE                          Type to declare json (de)serialization functions for.
 * \param PREFIX                        Optional function declaration prefix, e.g. <tt>inline</tt>.
 * \note                                This macro generates function declarations only.
 *                                      Definitions still have to be supplied.
 */
#define QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(TYPE, ... /* PREFIX */)         \
__VA_ARGS__ void serialize(const TYPE &value, QJsonValue *target);              \
__VA_ARGS__ bool deserialize(const QJsonValue &value, TYPE *target);


/**
 * This macro generates the necessary boilerplate to (de)serialize struct types.
 * It uses field names for JSON keys.
 *
 * \param TYPE                          Struct type to define (de)serialization functions for.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be (de)serialized.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
    QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(TYPE, BOOST_PP_SEQ_TRANSFORM(QN_CLASS_FROM_STRUCT_JSON_FIELD_I, TYPE, FIELD_SEQ), ##__VA_ARGS__)

#define QN_CLASS_FROM_STRUCT_JSON_FIELD_I(R, TYPE, FIELD)                       \
    (&TYPE::FIELD, &TYPE::FIELD, BOOST_PP_STRINGIZE(FIELD))


#define QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
__VA_ARGS__ void serialize(const TYPE &value, QJsonValue *target) {             \
    using namespace QJsonAccessors;                                             \
    QJsonObject result;                                                         \
    BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_CLASS_JSON_SERIALIZATION_STEP_I, ~, FIELD_SEQ) \
    *target = result;                                                           \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(const QJsonValue &value, TYPE *target) {           \
    using namespace QJsonAccessors;                                             \
    if(value.type() != QJsonValue::Object)                                      \
        return false;                                                           \
    QJsonObject object = value.toObject();                                      \
                                                                                \
    TYPE result;                                                                \
    BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_CLASS_JSON_DESERIALIZATION_STEP_I, ~, FIELD_SEQ) \
    *target = result;                                                           \
    return true;                                                                \
}

#define QN_DEFINE_CLASS_JSON_SERIALIZATION_STEP_I(R, DATA, FIELD)               \
    QN_DEFINE_CLASS_JSON_SERIALIZATION_STEP_II FIELD

#define QN_DEFINE_CLASS_JSON_SERIALIZATION_STEP_II(GETTER, SETTER, NAME)        \
    QJson::serialize(getMember(value, GETTER), QStringLiteral(NAME), &result);

#define QN_DEFINE_CLASS_JSON_DESERIALIZATION_STEP_I(R, DATA, FIELD)             \
    QN_DEFINE_CLASS_JSON_DESERIALIZATION_STEP_II FIELD

#define QN_DEFINE_CLASS_JSON_DESERIALIZATION_STEP_II(GETTER, SETTER, NAME)      \
    {                                                                           \
        typedef decltype(getMember(result, GETTER)) member_type;                \
        if(!QJsonDetail::deserializeMember(object, QStringLiteral(NAME), result, SETTER, static_cast<const member_type *>(NULL))) \
            return false;                                                       \
    }


#define QN_DEFINE_ENUM_JSON_SERIALIZATION_FUNCTIONS(TYPE, ... /* PREFIX */)     \
__VA_ARGS__ void serialize(const TYPE &value, QJsonValue *target) {             \
    *target = QnLexical::serialized(value);                                     \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(const QJsonValue &value, TYPE *target) {           \
    QString string;                                                             \
    return QJson::deserialize(value, &string) && QnLexical::deserialize(string, target); \
}


#define QN_DEFINE_ENUM_CAST_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(TYPE, ... /* PREFIX */) \
    QN_DEFINE_ENUM_CAST_LEXICAL_SERIALIZATION_FUNCTIONS(TYPE, ##__VA_ARGS__)    \
    QN_DEFINE_ENUM_JSON_SERIALIZATION_FUNCTIONS(TYPE, ##__VA_ARGS__)

#define QN_DEFINE_ENUM_MAPPED_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(TYPE, ... /* PREFIX */) \
    QN_DEFINE_ENUM_MAPPED_LEXICAL_SERIALIZATION_FUNCTIONS(TYPE, ##__VA_ARGS__)  \
    QN_DEFINE_ENUM_JSON_SERIALIZATION_FUNCTIONS(TYPE, ##__VA_ARGS__)

#else // Q_MOC_RUN

/* Qt moc chokes on our macro hell, so we make things easier for it. */
#define QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(...)
#define QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(...)
#define QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(...)
#define QN_DEFINE_ENUM_JSON_SERIALIZATION_FUNCTIONS(...)

#endif // Q_MOC_RUN

#include "json_functions.h"

#endif // QN_JSON_H
