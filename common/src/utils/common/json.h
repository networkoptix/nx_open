#ifndef QN_JSON_H
#define QN_JSON_H

#include <cassert>

#include <QtCore/QVariant>
#include <QtCore/QStringList>
#include <QtCore/QList>
#include <QtCore/QVector>


/* Free-standing (de)serialization functions are picked up via ADL by
 * the actual implementation. Feel free to add them for your own types. */

#define QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(TYPE)                          \
inline void serialize(const TYPE &value, QVariant *target) {                    \
    *target = value;                                                            \
}                                                                               \
                                                                                \
inline bool deserialize(const QVariant &value, TYPE *target) {                  \
    if(value.type() != qMetaTypeId<TYPE>())      /* TODO: WRONG!!!    */        \
        return false;                                                           \
                                                                                \
    *target = value.value<TYPE>();                                              \
    return true;                                                                \
}

/* These are the types supported by QJson inside a QVariant.
 * See serializer.cpp from QJson for details. */
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(QVariantMap);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(QVariantList);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(QString);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(double);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(bool);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(int);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(unsigned int);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(long long);
QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS(unsigned long long);

#undef QN_DEFINE_DIRECT_SERIALIZATION_FUNCTIONS


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


#endif // QN_JSON_H
