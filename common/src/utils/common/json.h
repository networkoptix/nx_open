#ifndef QN_JSON_H
#define QN_JSON_H

#include <cassert>

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/cat.hpp>

#include <QtCore/QVariant>
#include <QtCore/QStringList>
#include <QtCore/QList>
#include <QtCore/QVector>
#include <QtCore/QMap>
#include <QtCore/QHash>

namespace QJson {

    template<class T>
    void serialize(const T &value, QVariant *target);

    template<class T>
    void serialize(const T &value, const char *key, QVariantMap *target);

    template<class T>
    bool deserialize(const QVariant &value, T *target);

    template<class T>
    bool deserialize(const QVariantMap &map, const char *key, T *target);

} // namespace QJson

namespace QJson_detail {
    void serialize_json(const QVariant &value, QByteArray *target);
    bool deserialize_json(const QByteArray &value, QVariant *target);

    template<class T, class List>
    void serialize_list(const List &value, QVariant *target) {
        QVariantList result;
        result.reserve(value.size());

        foreach(const T &element, value) {
            result.push_back(QVariant());
            QJson::serialize(element, &result.back());
        }

        *target = result;
    }

    template<class T, class List>
    bool deserialize_list(const QVariant &value, List *target) {
        if(value.type() != QVariant::List)
            return false;

        QVariantList list = value.toList();
        List result;
        result.reserve(list.size());

        foreach(const QVariant &variant, list) {
            result.push_back(T());
            if(!QJson::deserialize(variant, &result.back()))
                return false;
        }

        *target = result;
        return true;
    }

    template<class T, class Map>
    void serialize_string_map(const Map &value, QVariant *target) {
        QVariantMap result;

        for(typename Map::const_iterator pos = value.begin(); pos != value.end(); pos++) {
            typename QVariantMap::iterator ipos = result.insert(pos.key(), QVariant());
            QJson::serialize(pos.value(), &ipos.value());
        }

        *target = result;
    }

    template<class T, class Map>
    void deserialize_string_map(const QVariant &value, Map *target) {
        if(value.type() != QVariant::Map)
            return false;

        QVariantMap map = value.toMap();
        Map result;

        for(typename QVariantMap::const_iterator pos = map.begin(); pos != map.end(); pos++) {
            typename Map::iterator ipos = result.insert(pos.key(), typename Map::mapped_type());
            if(!QJson::deserialize(pos.value(), &ipos.value()))
                return false;
        }

        *target = result;
        return true;
    }

    template<class T, class Map>
    void serialize_any_map(const Map &value, QVariant *target) {
        QVariantList result;

        for(typename Map::const_iterator pos = value.begin(); pos != value.end(); pos++) {
            QVariantMap map;
            QJson::serialize(pos.key(), "key", &map);
            QJson::serialize(pos.value(), "value", &map);
            result.push_back(map);
        }

        *target = result;
    }

    template<class T, class Map>
    void deserialize_any_map(const QVariant &value, Map *target) {
        if(value.type() != QVariant::List)
            return false;

        QVariantList list = value.toList();
        Map result;

        foreach(const QVariant &variant, list) {
            if(variant.type() != QVariant::Map)
                return false;

            QVariantMap map = variant.toMap();

            typename Map::key_type key;
            if(!QJson::deserialize(map, "key", &key))
                return false;

            typename Map::iterator ipos = result.insert(key, typename Map::mapped_type());
            if(!QJson::deserialize(map, "value", &ipos.value()))
                return false;
        }

        *target = result;
        return true;
    }

    template<class T>
    void serialize_value(const T &value, QVariant *target) {
        serialize(value, target); /* That's the place where ADL kicks in. */
    }

    template<class T>
    bool deserialize_value(const QVariant &value, T *target) {
        return deserialize(value, target); /* That's the place where ADL kicks in. */
    }

} // namespace QJson_detail


namespace QJson {
    template<class T>
    void serialize(const T &value, QVariant *target) {
        assert(target);

        QJson_detail::serialize_value(value, target);
    }

    template<class T>
    void serialize(const T &value, const char *key, QVariantMap *target) {
        assert(target);

        QJson::serialize(value, &(*target)[QLatin1String(key)]);
    }

    template<class T>
    bool deserialize(const QVariant &value, T *target) {
        assert(target);

        return QJson_detail::deserialize_value(value, target);
    }

    template<class T>
    bool deserialize(const QVariantMap &map, const char *key, T *target) {
        QVariantMap::const_iterator pos = map.find(QLatin1String(key));
        if(pos == map.end()) {
            return false;
        } else {
            return QJson::deserialize(*pos, target);
        }
    }

    template<class T>
    void serialize(const T &value, QByteArray *target) {
        assert(target);

        QVariant variant;
        QJson_detail::serialize_value(value, &variant);
        QJson_detail::serialize_json(variant, target);
    }

    template<class T>
    bool deserialize(const QByteArray &value, T *target) {
        assert(target);

        QVariant variant;
        if(!QJson_detail::deserialize_json(value, &variant))
            return false;
        return QJson_detail::deserialize_value(variant, target);
    }

} // namespace QJson


/* Free-standing (de)serialization functions are picked up via ADL by
 * the actual implementation. Feel free to add them for your own types. */


/* The following serialization functions are for types supported by QJson inside a QVariant.
 * See serializer.cpp from QJson for details. */

#define QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(TYPE)                          \
inline void serialize(const TYPE &value, QVariant *target) {                    \
    *target = QVariant::fromValue<TYPE>(value);                                 \
}                                                                               \
                                                                                \
inline bool deserialize(const QVariant &value, TYPE *target) {                  \
    QVariant localValue = value;                                                \
    if(!localValue.convert(static_cast<QVariant::Type>(qMetaTypeId<TYPE>())))   \
        return false;                                                           \
                                                                                \
    *target = localValue.value<TYPE>();                                         \
    return true;                                                                \
}

QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(QString);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(double);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(bool);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(char);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(unsigned char);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(short);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(unsigned short);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(int);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(unsigned int);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(long);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(unsigned long);
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

struct QUuid;
void serialize(const QUuid &value, QVariant *target);
bool deserialize(const QVariant &value, QUuid *target);

void serialize(const QColor &value, QVariant *target);
bool deserialize(const QVariant &value, QColor *target);


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


#endif // QN_JSON_H
