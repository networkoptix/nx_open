#ifndef QN_SERIALIZATION_SQL_H
#define QN_SERIALIZATION_SQL_H

#include <cassert>

#include <QtSql/QSqlRecord>
#include <QtSql/QSqlQuery>

#include <nx/utils/conversion_wrapper.h>
#include <nx/utils/log/assert.h>

#include "sql_fwd.h"
#include "sql_index_mapping.h"

namespace QnSqlDetail {
    template<class T>
    void bind_internal(const T &value, QSqlQuery *target) {
        bind(value, target);
    }

    template<class T>
    QnSqlIndexMapping mapping_internal(const QSqlQuery &query, const T *dummy) {
        return mapping(disable_user_conversions(query), dummy);
    }

    template<class T>
    void fetch_internal(const QnSqlIndexMapping &mapping, const QSqlRecord &value, T *target) {
        fetch(mapping, disable_user_conversions(value), target);
    }

    template<class T>
    void serialize_field_internal(const T &value, QVariant *target) {
        serialize_field(value, target);
    }

    template<class T>
    void deserialize_field_internal(const QVariant &value, T *target) {
        deserialize_field(disable_user_conversions(value), target);
    }

    struct TrueChecker {
        template<class T>
        bool operator()(const T &) const { return true; }
    };

} // namespace QnSqlDetail


namespace QnSql {
    template<class T>
    void bind(const T &value, QSqlQuery *target) {
        NX_ASSERT(target);

        QnSqlDetail::bind_internal(value, target);
    }

    template<class T>
    QnSqlIndexMapping mapping(const QSqlQuery &query, const T *dummy = NULL) {
        return QnSqlDetail::mapping_internal(query, dummy);
    }

    template<class T>
    void fetch(const QnSqlIndexMapping &mapping, const QSqlRecord &value, T *target) {
        NX_ASSERT(target);

        QnSqlDetail::fetch_internal(mapping, value, target);
    }

    template<class List, class Predicate>
    void fetch_many_if(QSqlQuery &query, List *target, const Predicate &predicate) {
        typedef typename List::value_type value_type;

        QSqlRecord record = query.record();
        QnSqlIndexMapping mapping = QnSql::mapping<value_type>(query);

        while(query.next()) {
            target->push_back(value_type());
            QnSql::fetch(mapping, query.record(), &target->back());
            if(!predicate(target->back()))
                target->pop_back();
        }
    }

    template<class List>
    void fetch_many(QSqlQuery &query, List *target) {
        fetch_many_if(query, target, QnSqlDetail::TrueChecker());
    }

    template<class T>
    void serialize_field(const T &value, QVariant *target) {
        NX_ASSERT(target);

        QnSqlDetail::serialize_field_internal(value, target);
    }

    template<class T>
    QVariant serialized_field(const T &value) {
        QVariant result;
        QnSql::serialize_field(value, &result);
        return result;
    }

    template<class T>
    void deserialize_field(const QVariant &value, T *target) {
        NX_ASSERT(target);

        QnSqlDetail::deserialize_field_internal(value, target);
    }

    template<class T>
    T deserialized_field(const QVariant &value) {
        T result;
        QnSql::deserialize_field(value, &result);
        return result;
    }

} // namespace QnSql

  /* Some fields are not meant to be bound or fetched. */
template<class T, class Allocator>
inline void serialize_field(const std::vector<T, Allocator> &, QVariant *) { return; }
template<class Key, class T, class Predicate, class Allocator>
inline void serialize_field(const std::map<Key, T, Predicate, Allocator> &, QVariant *) { return; }
template<class Key, class T>
inline void serialize_field(const QMap<Key, T> &, QVariant *) { return; }
template<class T>
inline void serialize_field(const QList<T> &, QVariant *) { return; }

template<class T, class Allocator>
inline void deserialize_field(const QVariant &, std::vector<T, Allocator> *) { return; }
template<class Key, class T, class Predicate, class Allocator>
inline void deserialize_field(const QVariant &, std::map<Key, T, Predicate, Allocator> *) { return; }
template<class Key, class T>
inline void deserialize_field(const QVariant &, QMap<Key, T> *) { return; }
template<class T>
inline void deserialize_field(const QVariant &, QList<T> *) { return; }

#endif // QN_SERIALIZATION_SQL_H
