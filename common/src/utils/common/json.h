#ifndef QN_JSON_H
#define QN_JSON_H

#include <cassert>

#include <boost/preprocessor/seq/for_each.hpp>

#include <QtCore/QVariant>
#include <QtCore/QStringList>
#include <QtCore/QList>
#include <QtCore/QVector>

namespace QJson_detail {
    void serialize_json(const QVariant &value, QByteArray *target);
    bool deserialize_json(const QByteArray &value, QVariant *target);

    template<class T, class List>
    void serialize_list(const List &value, QVariant *target) {
        QVariantList result;
        result.reserve(value.size());

        foreach(const T &element, value) {
            result.push_back(QVariant());
            serialize(element, &result.back());
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
            if(!deserialize(variant, &result.back()))
                return false;
        }

        *target = result;
        return true;
    }

    template<class T>
    void serialize_value(const T &value, QVariant *target) {
        serialize(value, target);
    }

    template<class T>
    bool deserialize_value(const QVariant &value, T *target) {
        return deserialize(value, target);
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

/* These are the types supported by QJson inside a QVariant.
 * See serializer.cpp from QJson for details. */
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

#define QN_DEFINE_LIST_SERIALIZATION_FUNCTIONS(TYPE)                            \
template<class T>                                                               \
void serialize(const TYPE<T> &value, QVariant *target) {                        \
    QJson_detail::serialize_list<T, TYPE<T> >(value, target);                   \
}                                                                               \
                                                                                \
template<class T>                                                               \
bool deserialize(const QVariant &value, TYPE<T> *target) {                      \
    return QJson_detail::deserialize_list<T, TYPE<T> >(value, target);          \
}                                                                               \

QN_DEFINE_LIST_SERIALIZATION_FUNCTIONS(QList);
QN_DEFINE_LIST_SERIALIZATION_FUNCTIONS(QVector);
#undef QN_DEFINE_LIST_SERIALIZATION_FUNCTIONS

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

#define QN_DEFINE_STRUCT_SERIALIZATION_STEP_I(R, DATA, FIELD)                   \
    QJson::serialize(value.FIELD, BOOST_PP_STRINGIZE(FIELD), &result);

#define QN_DEFINE_STRUCT_DESERIALIZATION_STEP_I(R, DATA, FIELD)                 \
    if(!QJson::deserialize(map, BOOST_PP_STRINGIZE(FIELD), &result.FIELD))      \
        return false;

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
	QnStorageSpaceData result;                                                  \
    BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_STRUCT_DESERIALIZATION_STEP_I, ~, FIELD_SEQ) \
	*target = result;                                                           \
	return true;                                                                \
}

#endif // QN_JSON_H
