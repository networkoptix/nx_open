#ifndef QN_JSON_H
#define QN_JSON_H

#include <cassert>

#ifndef Q_MOC_RUN
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/utility/enable_if.hpp>
#endif

#include <QtCore/QJsonValue>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include <QtCore/QStringList>
#include <QtCore/QList>
#include <QtCore/QVector>
#include <QtCore/QMap>
#include <QtCore/QHash>

namespace QJson_detail {
    void serialize_json(const QJsonValue &value, QByteArray *target);

    bool deserialize_json(const QByteArray &value, QJsonValue *target);

    template<class T>
    void serialize_value(const T &value, QJsonValue *target);

    template<class T>
    bool deserialize_value(const QJsonValue &value, T *target);

} // namespace QJson_detail


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

        QJson_detail::serialize_value(value, target);
    }

    template<class T>
    void serialize(const T &value, QJsonValueRef *target) {
        assert(target);

        QJsonValue jsonValue;
        QJson_detail::serialize_value(value, &jsonValue);
        *target = jsonValue;
    }

    template<class T>
    void serialize(const T &value, const char *key, QJsonObject *target) {
        assert(target);

        QJson::serialize(value, &(*target)[QLatin1String(key)]);
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
        QJson_detail::serialize_value(value, &jsonValue);
        QJson_detail::serialize_json(jsonValue, target);
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

        return QJson_detail::deserialize_value(value, target);
    }

    bool deserialize(const QJsonValueRef &value, T *target) {
        assert(target);

        return QJson_detail::deserialize_value(value, target);
    }

    template<class T>
    bool deserialize(const QJsonObject &value, const char *key, T *target, bool optional = false) {
        QJsonObject::const_iterator pos = value.find(QLatin1String(key));
        if(pos == value.end()) {
            return optional;
        } else {
            return QJson::deserialize(*pos, target);
        }
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
        if(!QJson_detail::deserialize_json(value, &jsonValue))
            return false;
        return QJson_detail::deserialize_value(jsonValue, target);
    }


    /**
     * Serializes the given value into a JSON string and returns it in the utf-8 format.
     *
     * \param value                     Value to serialize.
     * \returns                         Result JSON string.
     */
    template<class T>
    QString serialized(const T &value) {
        QByteArray result;
        QJson::serialize(value, &result);
        return QString::fromUtf8(result);
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


namespace QJson_detail {
    void serialize_json(const QVariant &value, QByteArray *target);
    bool deserialize_json(const QByteArray &value, QVariant *target);

    template<class T, class List>
    void serialize_list(const List &value, QJsonValue *target) {
        QJsonArray result;

        foreach(const T &element, value) {
            QJsonValue jsonValue;
            QJson::serialize(element, &jsonValue);
            result.push_back(jsonValue);
        }

        *target = result;
    }

    template<class T, class List>
    bool deserialize_list(const QJsonValue &value, List *target) {
        if(value.type() != QJsonValue::Array)
            return false;
        QJsonArray jsonArray = value.toArray();
        
        List result;
        result.reserve(jsonArray.size());

        foreach(const QJsonValue &jsonValue, jsonArray) {
            result.push_back(T());
            if(!QJson::deserialize(variant, &result.back()))
                return false;
        }

        *target = result;
        return true;
    }

    template<class T, class Map>
    void serialize_string_map(const Map &value, QJsonValue *target) {
        QJsonObject result;

        for(typename Map::const_iterator pos = value.begin(); pos != value.end(); pos++) {
            QJsonValue jsonValue;
            QJson::serialize(pos.value(), &jsonValue);
            result.insert(pos.key(), jsonValue);
        }

        *target = result;
    }

    template<class T, class Map>
    bool deserialize_string_map(const QJsonValue &value, Map *target) {
        if(value.type() != QJsonValue::Object)
            return false;
        QJsonObject jsonObject = value.toObject();
        
        Map result;

        for(typename QJsonObject::const_iterator pos = jsonObject.begin(); pos != jsonObject.end(); pos++)
            if(!QJson::deserialize(pos.value(), &(*result)[pos.key()]))
                return false;

        *target = result;
        return true;
    }

    template<class T, class Map>
    void serialize_any_map(const Map &value, QJsonValue *target) {
        QJsonArray result;

        for(typename Map::const_iterator pos = value.begin(); pos != value.end(); pos++) {
            QJsonObject jsonObject;
            QJson::serialize(pos.key(), "key", &jsonObject);
            QJson::serialize(pos.value(), "value", &jsonObject);
            result.push_back(jsonObject);
        }

        *target = result;
    }

    template<class T, class Map>
    bool deserialize_any_map(const QJsonValue &value, Map *target) {
        if(value.type() != QJsonValue::Array)
            return false;
        QJsonArray jsonArray = value.toArray();

        Map result;

        foreach(const QJsonValue &jsonValue, jsonArray) {
            if(jsonValue.type() != QJsonValue::Object)
                return false;
            QJsonObject jsonObject = jsonValue.toObject();

            typename Map::key_type key;
            if(!QJson::deserialize(jsonObject, "key", &key))
                return false;

            if(!QJson::deserialize(jsonObject, "value", &result[key]))
                return false;
        }

        *target = result;
        return true;
    }

    template<class T>
    void serialize_value(const T &value, QJsonValue *target) {
        serialize(value, target); /* That's the place where ADL kicks in. */
    }

    template<class T>
    bool deserialize_value(const QJsonValue &value, T *target) {
        return deserialize(value, target); /* That's the place where ADL kicks in. */
    }

} // namespace QJson_detail


/* Free-standing (de)serialization functions are picked up via ADL by
 * the actual implementation. Feel free to add them for your own types. */


#define QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(TYPE, JSON_TYPE, JSON_GETTER)  \
inline void serialize(const TYPE &value, QJsonValue *target) {                  \
    *target = QJsonValue(value);                                                \
}                                                                               \
                                                                                \
inline bool deserialize(const QJsonValue &value, TYPE *target) {                \
    if(value.type() != QJsonValue::JSON_TYPE) {                                 \
        return false;                                                           \
    } else {                                                                    \
        *target = value.JSON_GETTER();                                          \
        return true;                                                            \
    }                                                                           \
}

QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(QString, String, toString)
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(double, Double, toDouble)
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(bool, Bool, toBool)
#undef QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS

#define QN_DEFINE_NUMERIC_SERIALIZATION_FUNCTIONS(TYPE)                         \
inline void serialize(const TYPE &value, QJsonValue *target) {                  \
    QJson::serialize(static_cast<double>(value), target);                       \
}                                                                               \
                                                                                \
inline bool deserialize(const QJsonValue &value, TYPE *target) {                \
    double tmp;                                                                 \
    if(!QJson::deserialize(value, &tmp))                                        \
        return false;                                                           \
                                                                                \
    *target = static_cast<TYPE>(tmp);                                           \
    return true;                                                                \
}

QN_DEFINE_NUMERIC_SERIALIZATION_FUNCTIONS(char);
QN_DEFINE_NUMERIC_SERIALIZATION_FUNCTIONS(unsigned char);
QN_DEFINE_NUMERIC_SERIALIZATION_FUNCTIONS(short);
QN_DEFINE_NUMERIC_SERIALIZATION_FUNCTIONS(unsigned short);
QN_DEFINE_NUMERIC_SERIALIZATION_FUNCTIONS(int);
QN_DEFINE_NUMERIC_SERIALIZATION_FUNCTIONS(unsigned int);
#undef QN_DEFINE_NUMERIC_SERIALIZATION_FUNCTIONS

inline void serialize(const TYPE &value, QJsonValue *target) {
    *target = QJsonValue(QString::number(value));
}

inline bool deserialize(const QJsonValue &value, TYPE *target) {
    /* Support both strings and doubles during deserialization, just to feel safe. */
    if(value.type() == QJsonValue::Double) {
        *target = value.toDouble();
        return true;
    }

    if(value.type() == QJsonValue::String) {

    }

}

QN_DEFINE_CONVERSION_SERIALIZATION_FUNCTIONS(long);
QN_DEFINE_CONVERSION_SERIALIZATION_FUNCTIONS(unsigned long);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(long long);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(unsigned long long); 
#undef QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS


/* Some serialization functions for common Qt types follow. */

#define QN_DEFINE_CONTAINER_SERIALIZATION_FUNCTIONS(TYPE, TPL_DEF, TPL_ARG, IMPL) \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF)>                                          \
void serialize(const TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> &value, QVariant *target) { \
    QJson_detail::BOOST_PP_CAT(serialize_, IMPL)<T, TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> >(value, target); \
}                                                                               \
                                                                                \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF)>                                          \
bool deserialize(const QVariant &value, TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> *target) { \
    return QJson_detail::BOOST_PP_CAT(deserialize_, IMPL)<T, TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> >(value, target); \
}                                                                               \

QN_DEFINE_CONTAINER_SERIALIZATION_FUNCTIONS(QList, (class T), (T), list);
QN_DEFINE_CONTAINER_SERIALIZATION_FUNCTIONS(QVector, (class T), (T), list);
QN_DEFINE_CONTAINER_SERIALIZATION_FUNCTIONS(QMap, (class T), (QString, T), string_map);
QN_DEFINE_CONTAINER_SERIALIZATION_FUNCTIONS(QHash, (class T), (QString, T), string_map);
QN_DEFINE_CONTAINER_SERIALIZATION_FUNCTIONS(QMap, (class Key, class T), (Key, T), any_map);
QN_DEFINE_CONTAINER_SERIALIZATION_FUNCTIONS(QHash, (class Key, class T), (Key, T), any_map);
#undef QN_DEFINE_CONTAINER_SERIALIZATION_FUNCTIONS

/* Serialization can actually fail for QVariant containers because of types
 * unknown to QJson in them.
 * 
 * So we work that around by providing our own (de)serialization functions.
 * Note that these are not perfectly symmetric, e.g. deserialization of 
 * previously serialized QStringList will give a QVariantList. */

void serialize(const QVariant &value, QVariant *target);
bool deserialize(const QVariant &value, QVariant *target);

void serialize(const QVariantList &value, QVariant *target);
bool deserialize(const QVariant &value, QVariantList *target);

void serialize(const QVariantMap &value, QVariant *target);
bool deserialize(const QVariant &value, QVariantMap *target);


/* Serialization generators for common user cases follow. */

/**
 * \param TYPE                          Type to declare (de)serialization functions for.
 * \param PREFIX                        Optional function declaration prefix, e.g. <tt>inline</tt>.
 * \note                                This macro generates function declarations only.
 *                                      Definitions still have to be supplied.
 */
#define QN_DECLARE_SERIALIZATION_FUNCTIONS(TYPE, ... /* PREFIX */)              \
__VA_ARGS__ void serialize(const TYPE &value, QVariant *target);                \
__VA_ARGS__ bool deserialize(const QVariant &value, TYPE *target);


/**
 * This macro generates the necessary boilerplate to (de)serialize struct types.
 * It uses field names for JSON keys.
 *
 * \param TYPE                          Struct type to define (de)serialization functions for.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be (de)serialized.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
__VA_ARGS__ void serialize(const TYPE &value, QVariant *target) {               \
    QVariantMap result;                                                         \
    BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_STRUCT_SERIALIZATION_STEP_I, ~, FIELD_SEQ)  \
    *target = result;                                                           \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(const QVariant &value, TYPE *target) {             \
    if(value.type() != QVariant::Map)                                           \
        return false;                                                           \
    QVariantMap map = value.toMap();                                            \
                                                                                \
    TYPE result;                                                                \
    BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_STRUCT_DESERIALIZATION_STEP_I, ~, FIELD_SEQ) \
    *target = result;                                                           \
    return true;                                                                \
}

#define QN_DEFINE_STRUCT_SERIALIZATION_STEP_I(R, DATA, FIELD)                   \
    QJson::serialize(value.FIELD, BOOST_PP_STRINGIZE(FIELD), &result);

#define QN_DEFINE_STRUCT_DESERIALIZATION_STEP_I(R, DATA, FIELD)                 \
    if(!QJson::deserialize(map, BOOST_PP_STRINGIZE(FIELD), &result.FIELD))      \
    return false;


/**
 * This macro generates the necessary boilerplate to (de)serialize class types.
 * It uses field names for JSON keys.
 *
 * \param TYPE                          Struct type to define (de)serialization functions for.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be (de)serialized.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_DEFINE_CLASS_SERIALIZATION_FUNCTION(TYPE, FIELD_SEQ, ... /* PREFIX */) \
__VA_ARGS__ void serialize(const TYPE &value, QVariant *target) {               \
    QVariantMap result;                                                         \
    BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_CLASS_SERIALIZATION_STEP_I, ~, FIELD_SEQ)  \
    *target = result;                                                           \
}

#define QN_DEFINE_CLASS_DESERIALIZATION_FUNCTION(TYPE, FIELD_SEQ, FIELDTYPE, ... /* PREFIX */) \
__VA_ARGS__ bool deserialize(const QVariant &value, TYPE *target) {             \
    if(value.type() != QVariant::Map)                                           \
        return false;                                                           \
    QVariantMap map = value.toMap();                                            \
                                                                                \
    TYPE result;                                                                \
    BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_CLASS_DESERIALIZATION_STEP_I, FIELDTYPE, FIELD_SEQ) \
    *target = result;                                                           \
    return true;                                                                \
}

#define QN_DEFINE_CLASS_SERIALIZATION_STEP_I(R, DATA, FIELD)                    \
    QJson::serialize(value.FIELD(), BOOST_PP_STRINGIZE(FIELD), &result);

#define QN_SET_FIELD(FIELD) result.set##FIELD(v);

#define QN_DEFINE_CLASS_DESERIALIZATION_STEP_I(R, DATA, FIELD)                  \
{   DATA v;                                        \
    if(!QJson::deserialize(map, QByteArray(BOOST_PP_STRINGIZE(FIELD)).toLower(), &v))        \
        return false;                                                           \
    QN_SET_FIELD(FIELD) \
}

#endif // QN_JSON_H
