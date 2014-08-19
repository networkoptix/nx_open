#ifndef QN_SERIALIZATION_JSON_FUNCTIONS_H
#define QN_SERIALIZATION_JSON_FUNCTIONS_H

#include <vector>
#include <set>
#include <map>
#include <limits>

#include <boost/preprocessor/tuple/enum.hpp>

#include <QtCore/QSize>
#include <QtCore/QSizeF>
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtCore/QUuid>
#include <QtCore/QUrl>
#include <QtCore/QtNumeric>
#include <QtCore/QJsonArray>
#include <QtGui/QColor>
#include <QtGui/QRegion>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>
#include <QtGui/QFont>

#include <utils/common/collection.h>
#include <utils/common/latin1_array.h>

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

inline bool deserialize(QnJsonContext *, const QJsonValue &value, double *target) {
    if(value.type() == QJsonValue::Double) {
        *target = value.toDouble();
        return true;
    } else if(value.type() == QJsonValue::Null) {
        /* Strictly speaking, that's either a nan, or a +-inf, 
         * but at this point we cannot say what it really was. */
        *target = qQNaN(); 
        return true;
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



namespace QJsonDetail {

    template<class Element, class Tag>
    void serialize_collection_element(QnJsonContext *ctx, const Element &element, QJsonValue *target, const Tag &) {
        QJson::serialize(ctx, element, target);
    }

    template<class Element>
    void serialize_collection_element(QnJsonContext *ctx, const Element &element, QJsonValue *target, const QnCollection::map_tag &) {
        QJsonObject map;
        QJson::serialize(ctx, element.first, lit("key"), &map);
        QJson::serialize(ctx, element.second, lit("value"), &map);
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

        *target = result;
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
        if(!QJson::deserialize(ctx, element, lit("key"), &key))
            return false;

        if(!QJson::deserialize(ctx, element, lit("value"), &(*target)[key]))
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

    template<class Map>
    void serialize_string_map(QnJsonContext *ctx, const Map &value, QJsonValue *target) {
        QJsonObject result;

        for(auto pos = boost::begin(value); pos != boost::end(value); pos++) {
            QJsonValue jsonValue;
            QJson::serialize(ctx, pos->second, &jsonValue);
            result.insert(pos->first, jsonValue);
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
            if(!QJson::deserialize(ctx, pos.value(), &(*target)[pos.key()]))
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
QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS(bool,         Bool,   toBool)
QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS(QJsonArray,   Array,  toArray)
QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS(QJsonObject,  Object, toObject)
#undef QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS


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
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(QVector, (class T), (T), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(QVarLengthArray, (class T, int N), (T, N), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(QMap, (class Key, class T), (Key, T), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(QHash, (class Key, class T), (Key, T), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(std::vector, (class T, class Allocator), (T, Allocator), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(std::set, (class Key, class Predicate, class Allocator), (Key, Predicate, Allocator), collection);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(std::map, (class Key, class T, class Predicate, class Allocator), (Key, T, Predicate, Allocator), collection);

QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(QMap, (class T), (QString, T), string_map);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(QHash, (class T), (QString, T), string_map);
QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS(std::map, (class T, class Predicate, class Allocator), (QString, T, Predicate, Allocator), string_map);

#undef QN_DEFINE_COLLECTION_JSON_SERIALIZATION_FUNCTIONS
#endif // Q_MOC_RUN


namespace QJsonDetail {

    template<class T>
    void serialize_enum(QnJsonContext *ctx, const T &value, QJsonValue *target, typename std::enable_if<QnLexical::is_numerically_serializable<T>::value>::type * = NULL) {
        QnSerialization::check_enum_binary<T>();

        ::serialize(ctx, static_cast<qint32>(value), target); /* Note the direct call instead of invocation through QJson. */
    }

    template<class T>
    bool deserialize_enum(QnJsonContext *ctx, const QJsonValue &value, T *target, typename std::enable_if<QnLexical::is_numerically_serializable<T>::value>::type * = NULL) {
        QnSerialization::check_enum_binary<T>();

        /* Older version did it with lexical functions, so we have to support it. 
         * Plus there are types that are numeric, but can also be deserialized from a string. */
        if(value.type() == QJsonValue::String)
            return QnLexical::deserialize(value.toString(), target);

        qint32 tmp;
        if(!::deserialize(ctx, value, &tmp)) /* Note the direct call instead of invocation through QJson. */
            return false;
        *target = static_cast<T>(tmp);
        return true;
    }

    template<class T>
    void serialize_enum(QnJsonContext *ctx, const T &value, QJsonValue *target, typename std::enable_if<!QnLexical::is_numerically_serializable<T>::value>::type * = NULL) {
        QString tmp;
        QnLexical::serialize(value, &tmp);
        ::serialize(ctx, tmp, target); /* Note the direct call instead of invocation through QJson. */
    }

    template<class T>
    bool deserialize_enum(QnJsonContext *ctx, const QJsonValue &value, T *target, typename std::enable_if<!QnLexical::is_numerically_serializable<T>::value>::type * = NULL) {
        QString tmp;
        return ::deserialize(ctx, value, &tmp) && QnLexical::deserialize(tmp, target);
    }

} // namespace QJsonDetail


template<class T>
void serialize(QnJsonContext *ctx, const T &value, QJsonValue *target, typename std::enable_if<QnSerialization::is_enum_or_flags<T>::value>::type * = NULL) {
    QJsonDetail::serialize_enum(ctx, value, target);
}

template<class T>
bool deserialize(QnJsonContext *ctx, const QJsonValue &value, T *target, typename std::enable_if<QnSerialization::is_enum_or_flags<T>::value>::type * = NULL) {
    return QJsonDetail::deserialize_enum(ctx, value, target);
}


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QByteArray)(QnLatin1Array)(QColor)(QBrush)(QSize)(QSizeF)(QRect)(QRectF)(QPoint)(QPointF)(QRegion)(QVector2D)(QVector3D)(QVector4D)(QUuid)(QUrl)(QFont), (json))

void qnJsonFunctionsUnitTest();

#endif // QN_SERIALIZATION_JSON_FUNCTIONS_H
