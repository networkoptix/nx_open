#ifndef QN_JSON_H
#define QN_JSON_H

#include <cassert>

#include <QtCore/QVariant>
#include <QtCore/QStringList>
#include <QtCore/QBuffer>

/* Free-standing (de)serialization functions are picked up via ADL by
 * the actual implementation. Feel free to add them for your own types. */

void serialize(const QStringList &value, QVariant *target);
bool deserialize(const QVariant &value, QStringList *target);

void serialize(const QVariantMap &value, QVariant *target);
bool deserialize(const QVariant &value, QVariantMap *target);


namespace QJson {
    namespace detail {
        void json_serialize(const QVariant &value, QByteArray *target);
        bool json_deserialize(const QByteArray &value, QVariant *target);
    } // namespace detail

    template<class T>
    void serialize(const T &value, QByteArray *target) {
        using ::serialize; /* Let ADL kick in. */

        assert(target);

        QVariant variant;
        serialize(value, &variant);
        detail::json_serialize(variant, target);
    }

    template<class T>
    bool deserialize(const QByteArray &value, T *target) {
        using ::deserialize; /* Let ADL kick in. */

        assert(target);

        QVariant variant;
        if(!detail::json_deserialize(value, &variant))
            return false;
        return deserialize(variant, target);
    }

} // namespace QJson

#endif // QN_JSON_H
