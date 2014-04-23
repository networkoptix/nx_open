#ifndef QN_SERIALIZATION_JSON_FUNCTIONS_H
#define QN_SERIALIZATION_JSON_FUNCTIONS_H

#include <QtCore/QList>
#include <QtCore/QVector>
#include <QtCore/QMap>
#include <QtCore/QHash>
#include <QtCore/QVarLengthArray>
#include <QtCore/QSize>
#include <QtCore/QSizeF>
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtCore/QUuid>
#include <QtCore/QtNumeric>
#include <QtGui/QColor>
#include <QtGui/QRegion>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>
#include <QtGui/QFont>

#include "json.h"
#include "lexical.h"

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
    template<class T, class List>
    void serialize_list(QnJsonContext *ctx, const List &value, QJsonValue *target) {
        QJsonArray result;

        foreach(const T &element, value) {
            QJsonValue jsonValue;
            QJson::serialize(ctx, element, &jsonValue);
            result.push_back(jsonValue);
        }

        *target = result;
    }

    // TODO: #Elric we've given up the idea that failed deserealization shouldn't touch the target.
    template<class T, class List>
    bool deserialize_list(QnJsonContext *ctx, const QJsonValue &value, List *target) {
        if(value.type() != QJsonValue::Array)
            return false;
        QJsonArray jsonArray = value.toArray();
        
        List result;
        result.reserve(jsonArray.size());

        foreach(const QJsonValue &jsonValue, jsonArray) {
            result.push_back(T());
            if(!QJson::deserialize(ctx, jsonValue, &result.back()))
                return false;
        }

        *target = result;
        return true;
    }

    template<class T, class Map>
    void serialize_string_map(QnJsonContext *ctx, const Map &value, QJsonValue *target) {
        QJsonObject result;

        for(typename Map::const_iterator pos = value.begin(); pos != value.end(); pos++) {
            QJsonValue jsonValue;
            QJson::serialize(ctx, pos.value(), &jsonValue);
            result.insert(pos.key(), jsonValue);
        }

        *target = result;
    }

    template<class T, class Map>
    bool deserialize_string_map(QnJsonContext *ctx, const QJsonValue &value, Map *target) {
        if(value.type() != QJsonValue::Object)
            return false;
        QJsonObject jsonObject = value.toObject();
        
        Map result;

        for(typename QJsonObject::const_iterator pos = jsonObject.begin(); pos != jsonObject.end(); pos++)
            if(!QJson::deserialize(ctx, pos.value(), &result[pos.key()]))
                return false;

        *target = result;
        return true;
    }

    template<class T, class Map>
    void serialize_any_map(QnJsonContext *ctx, const Map &value, QJsonValue *target) {
        QJsonArray result;

        for(typename Map::const_iterator pos = value.begin(); pos != value.end(); pos++) {
            QJsonObject jsonObject;
            QJson::serialize(ctx, pos.key(), lit("key"), &jsonObject);
            QJson::serialize(ctx, pos.value(), lit("value"), &jsonObject);
            result.push_back(jsonObject);
        }

        *target = result;
    }

    template<class T, class Map>
    bool deserialize_any_map(QnJsonContext *ctx, const QJsonValue &value, Map *target) {
        if(value.type() != QJsonValue::Array)
            return false;
        QJsonArray jsonArray = value.toArray();

        Map result;

        foreach(const QJsonValue &jsonValue, jsonArray) {
            if(jsonValue.type() != QJsonValue::Object)
                return false;
            QJsonObject jsonObject = jsonValue.toObject();

            typename Map::key_type key;
            if(!QJson::deserialize(ctx, jsonObject, lit("key"), &key))
                return false;

            if(!QJson::deserialize(ctx, jsonObject, lit("value"), &result[key]))
                return false;
        }

        *target = result;
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


#define QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(TYPE, TPL_DEF, TPL_ARG, IMPL) \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF)>                                          \
void serialize(QnJsonContext *ctx, const TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> &value, QJsonValue *target) { \
    QJsonDetail::BOOST_PP_CAT(serialize_, IMPL)<T, TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> >(ctx, value, target); \
}                                                                               \
                                                                                \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF)>                                          \
bool deserialize(QnJsonContext *ctx, const QJsonValue &value, TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> *target) { \
    return QJsonDetail::BOOST_PP_CAT(deserialize_, IMPL)<T, TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> >(ctx, value, target); \
}                                                                               \

QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(QList, (class T), (T), list);
QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(QVector, (class T), (T), list);
QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(QVarLengthArray, (class T, int N), (T, N), list);
QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(QMap, (class T), (QString, T), string_map);
QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(QHash, (class T), (QString, T), string_map);
QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(QMap, (class Key, class T), (Key, T), any_map);
QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(QHash, (class Key, class T), (Key, T), any_map);
#undef QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS
#endif // Q_MOC_RUN

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QColor)(QBrush)(QSize)(QSizeF)(QRect)(QRectF)(QPoint)(QPointF)(QRegion)(QVector2D)(QVector3D)(QVector4D)(QUuid)(QFont), (json))

void qnJsonFunctionsUnitTest();

#endif // QN_SERIALIZATION_JSON_FUNCTIONS_H
