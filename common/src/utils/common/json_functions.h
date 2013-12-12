#ifndef JSON_UTILS_H
#define JSON_UTILS_H

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
#include <QtGui/QColor>
#include <QtGui/QRegion>

#include <utils/common/json.h>
#include <utils/common/lexical.h>


namespace QJsonDetail {
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
            if(!QJson::deserialize(jsonValue, &result.back()))
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
            if(!QJson::deserialize(pos.value(), &result[pos.key()]))
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
    void serialize_numeric(const T &value, QJsonValue *target) {
        QJson::serialize(static_cast<double>(value), target);
    }
    
    template<class T>
    bool deserialize_numeric(const QJsonValue &value, T *target) {
        double tmp;
        if(!QJson::deserialize(value, &tmp))
            return false;
        if(tmp < static_cast<double>(std::numeric_limits<T>::min()) || tmp > static_cast<double>(std::numeric_limits<T>::max()))
            return false;

        *target = static_cast<T>(tmp);
        return true;
    }

    template<class T>
    void serialize_numeric_string(const T &value, QJsonValue *target) {
        *target = QJsonValue(QnLexical::serialized(value));
    }

    template<class T>
    bool deserialize_numeric_string(const QJsonValue &value, T *target) {
        /* Support both strings and doubles during deserialization, just to feel safe. */
        if(value.type() == QJsonValue::Double) {
            return QJsonDetail::deserialize_numeric<T>(value, target);
        } else if(value.type() == QJsonValue::String) {
            return QnLexical::deserialize(value.toString(), target);
        } else {
            return false;
        }
    }

} // namespace QJsonDetail


/* Free-standing (de)serialization functions are picked up via ADL by
 * the actual implementation. Feel free to add them for your own types. */


inline void serialize(const QJsonValue &value, QJsonValue *target) {
    *target = value;
}

inline bool deserialize(const QJsonValue &value, QJsonValue *target) {
    *target = value;
    return true;
}


#define QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS(TYPE, JSON_TYPE, JSON_GETTER)  \
inline void serialize(const TYPE &value, QJsonValue *target) {                  \
    *target = QJsonValue(value);                                                \
}                                                                               \
                                                                                \
inline bool deserialize(const QJsonValue &value, TYPE *target) {                \
    if(value.type() != QJsonValue::JSON_TYPE)                                   \
        return false;                                                           \
                                                                                \
    *target = value.JSON_GETTER();                                              \
    return true;                                                                \
}

QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS(QString,      String, toString)
QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS(double,       Double, toDouble)
QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS(bool,         Bool,   toBool)
QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS(QJsonArray,   Array,  toArray)
QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS(QJsonObject,  Object, toObject)
#undef QN_DEFINE_DIRECT_JSON_SERIALIZATION_FUNCTIONS


#define QN_DEFINE_NUMERIC_CONVERSION_JSON_SERIALIZATION_FUNCTIONS(TYPE)         \
inline void serialize(const TYPE &value, QJsonValue *target) {                  \
    QJsonDetail::serialize_numeric<TYPE>(value, target);                        \
}                                                                               \
                                                                                \
inline bool deserialize(const QJsonValue &value, TYPE *target) {                \
    return QJsonDetail::deserialize_numeric<TYPE>(value, target);               \
}

QN_DEFINE_NUMERIC_CONVERSION_JSON_SERIALIZATION_FUNCTIONS(char);
QN_DEFINE_NUMERIC_CONVERSION_JSON_SERIALIZATION_FUNCTIONS(unsigned char);
QN_DEFINE_NUMERIC_CONVERSION_JSON_SERIALIZATION_FUNCTIONS(short);
QN_DEFINE_NUMERIC_CONVERSION_JSON_SERIALIZATION_FUNCTIONS(unsigned short);
QN_DEFINE_NUMERIC_CONVERSION_JSON_SERIALIZATION_FUNCTIONS(int);
QN_DEFINE_NUMERIC_CONVERSION_JSON_SERIALIZATION_FUNCTIONS(unsigned int);
QN_DEFINE_NUMERIC_CONVERSION_JSON_SERIALIZATION_FUNCTIONS(float);
#undef QN_DEFINE_NUMERIC_CONVERSION_JSON_SERIALIZATION_FUNCTIONS


#define QN_DEFINE_NUMERIC_STRING_JSON_SERIALIZATION_FUNCTIONS(TYPE)             \
inline void serialize(const TYPE &value, QJsonValue *target) {                  \
    QJsonDetail::serialize_numeric_string<TYPE>(value, target);                 \
}                                                                               \
                                                                                \
inline bool deserialize(const QJsonValue &value, TYPE *target) {                \
    return QJsonDetail::deserialize_numeric_string<TYPE>(value, target);        \
}

QN_DEFINE_NUMERIC_STRING_JSON_SERIALIZATION_FUNCTIONS(long)
QN_DEFINE_NUMERIC_STRING_JSON_SERIALIZATION_FUNCTIONS(unsigned long)
QN_DEFINE_NUMERIC_STRING_JSON_SERIALIZATION_FUNCTIONS(long long)
QN_DEFINE_NUMERIC_STRING_JSON_SERIALIZATION_FUNCTIONS(unsigned long long)
#undef QN_DEFINE_NUMERIC_STRING_JSON_SERIALIZATION_FUNCTIONS


#define QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(TYPE, TPL_DEF, TPL_ARG, IMPL) \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF)>                                          \
void serialize(const TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> &value, QJsonValue *target) { \
    QJsonDetail::BOOST_PP_CAT(serialize_, IMPL)<T, TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> >(value, target); \
}                                                                               \
                                                                                \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF)>                                          \
bool deserialize(const QJsonValue &value, TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> *target) { \
    return QJsonDetail::BOOST_PP_CAT(deserialize_, IMPL)<T, TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> >(value, target); \
}                                                                               \

QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(QList, (class T), (T), list);
QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(QVector, (class T), (T), list);
QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(QVarLengthArray, (class T, int N), (T, N), list);
QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(QMap, (class T), (QString, T), string_map);
QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(QHash, (class T), (QString, T), string_map);
QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(QMap, (class Key, class T), (Key, T), any_map);
QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS(QHash, (class Key, class T), (Key, T), any_map);
#undef QN_DEFINE_CONTAINER_JSON_SERIALIZATION_FUNCTIONS


QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QSize)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QSizeF)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QRect)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QRectF)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QPoint)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QPointF)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QUuid)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QColor)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QRegion)


void qnJsonFunctionsUnitTest();

#endif // JSON_UTILS_H
