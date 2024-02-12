// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_SERIALIZATION_SQL_FUNCTIONS_H
#define QN_SERIALIZATION_SQL_FUNCTIONS_H

#include <chrono>
#include <set>
#include <string>

#include <nx/utils/uuid.h>

#include "enum.h"
#include "sql.h"
#include "sql_macros.h"

namespace QnSqlDetail {
    template<class T>
    inline void serialize_field_direct(const T &value, QVariant *target) {
        *target = QVariant::fromValue<T>(value);
    }

    template<class T>
    inline void deserialize_field_direct(const QVariant &value, T *target) {
        *target = value.value<T>();
    }

} // namespace QnSqlDetail

#define QN_DEFINE_DIRECT_SQL_FIELD_FUNCTIONS(TYPE)                              \
inline void serialize_field(const TYPE &value, QVariant *target) {              \
    QnSqlDetail::serialize_field_direct(value, target);                         \
}                                                                               \
                                                                                \
inline void deserialize_field(const QVariant &value, TYPE *target) {            \
    QnSqlDetail::deserialize_field_direct(value, target);                       \
}

QN_DEFINE_DIRECT_SQL_FIELD_FUNCTIONS(short)
QN_DEFINE_DIRECT_SQL_FIELD_FUNCTIONS(unsigned short)
QN_DEFINE_DIRECT_SQL_FIELD_FUNCTIONS(int)
QN_DEFINE_DIRECT_SQL_FIELD_FUNCTIONS(unsigned int)
QN_DEFINE_DIRECT_SQL_FIELD_FUNCTIONS(long)
QN_DEFINE_DIRECT_SQL_FIELD_FUNCTIONS(unsigned long)
QN_DEFINE_DIRECT_SQL_FIELD_FUNCTIONS(long long)
QN_DEFINE_DIRECT_SQL_FIELD_FUNCTIONS(unsigned long long)
QN_DEFINE_DIRECT_SQL_FIELD_FUNCTIONS(float)
QN_DEFINE_DIRECT_SQL_FIELD_FUNCTIONS(double)
QN_DEFINE_DIRECT_SQL_FIELD_FUNCTIONS(QByteArray)
#undef QN_DEFINE_DIRECT_SQL_FIELD_FUNCTIONS

inline void serialize_field(const bool &value, QVariant *target) {
    *target = QVariant::fromValue<int>(value ? 1 : 0);
}

inline void deserialize_field(const QVariant &value, bool *target) {
    *target = value.toBool();
}


inline void serialize_field(const unsigned char &value, QVariant *target) {
    *target = QVariant::fromValue<unsigned int>(value);
}

inline void deserialize_field(const QVariant &value, unsigned char *target) {
    *target = value.value<unsigned int>();
}


inline void serialize_field(const signed char &value, QVariant *target) {
    *target = QVariant::fromValue<int>(value);
}

inline void deserialize_field(const QVariant &value, signed char *target) {
    *target = value.value<int>();
}


inline void serialize_field(const QString &value, QVariant *target) {
    *target = QVariant::fromValue<QString>(value.isEmpty() ? QLatin1String("") : value);
}

inline void serialize_field(const std::string &value, QVariant *target) {
    *target = QVariant::fromValue<QString>(QString::fromStdString(value));
}

inline void deserialize_field(const QVariant &value, QString *target) {
    *target = value.value<QString>();
}

inline void deserialize_field(const QVariant &value, std::string *target) {
    *target = value.value<QString>().toStdString();
}

inline void serialize_field(const nx::Uuid &value, QVariant *target) {
    *target = QVariant::fromValue<QByteArray>(value.toRfc4122());
}

inline void deserialize_field(const QVariant &value, nx::Uuid *target) {
    *target = nx::Uuid::fromRfc4122(value.value<QByteArray>());
}

inline void serialize_field(const std::chrono::milliseconds &value, QVariant *target) {
    *target = QVariant::fromValue<qint64>(value.count());
}

inline void deserialize_field(const QVariant &value, std::chrono::milliseconds *target) {
    *target = std::chrono::milliseconds(value.value<qint64>());
}

inline void serialize_field(const std::chrono::seconds& value, QVariant* target)
{
    *target = QVariant::fromValue<qint64>(value.count());
}

inline void deserialize_field(const QVariant& value, std::chrono::seconds* target)
{
    *target = std::chrono::seconds(value.value<qint64>());
}

template<size_t N, size_t M, typename T>
void serialize_field(const std::array<std::array<T, M>, N>& a, QVariant *target)
{
    QList<T> result;
    for (const auto& inner: a)
    {
        for (const auto& v: inner)
            result.append(v);
    }

    *target = QVariant(result);
}

template<size_t N, size_t M, typename T>
void deserialize_field(const QVariant &value,  std::array<std::array<T, M>, N>* a)
{
    NX_ASSERT(value.canConvert<QList<QVariant>>());
    const auto list = value.toList();
    NX_ASSERT(list.size() == M * N);
    for (size_t i = 0; i < N; ++i)
    {
        for (size_t j = 0; j < M; ++j)
        {
            NX_ASSERT(list[i * M + j].canConvert<T>());
            (*a)[i][j] = list[i * M + j].value<T>();
        }
    }
}

/**
 * Representing system_clock::time_point in SQL as milliseconds since epoch (1970-01-01T00:00).
 */
inline void serialize_field(const std::chrono::system_clock::time_point& value, QVariant* target) {
    using namespace std::chrono;

    const auto millisSinceEpoch =
        duration_cast<milliseconds>(value.time_since_epoch()).count();
    *target = QVariant::fromValue<qulonglong>(millisSinceEpoch);
}

inline void deserialize_field(const QVariant& value, std::chrono::system_clock::time_point* target) {
    const auto millisSinceEpoch = value.toULongLong();
    // Initializing with epoch.
    *target = std::chrono::system_clock::time_point();
    // Adding milliseconds since epoch.
    *target += std::chrono::milliseconds(millisSinceEpoch);
}

template<class T>
void serialize_field(const T &value, QVariant *target, typename std::enable_if<std::is_enum<T>::value>::type * = NULL) {
    QnSql::serialize_field(static_cast<qint32>(value), target);
}

template<class T>
void deserialize_field(const QVariant &value, T *target, typename std::enable_if<std::is_enum<T>::value>::type * = NULL) {
    qint32 tmp;
    QnSql::deserialize_field(value, &tmp);
    *target = static_cast<T>(tmp);
}


template<class T>
inline void serialize_field(const QFlags<T> &value, QVariant *target) {
    QnSql::serialize_field(static_cast<qint32>(value), target);
}

template<class T>
inline void deserialize_field(const QVariant &value, QFlags<T> *target) {
    qint32 tmp;
    QnSql::deserialize_field(value, &tmp);
    *target = static_cast<QFlags<T> >(tmp);
}

template<class T>
inline void serialize_field(const std::optional<T>& value, QVariant* target)
{
    if (!value)
    {
        *target = QVariant();
        return;
    }
    return serialize_field(*value, target);
}

template<class T>
inline void deserialize_field(const QVariant& value, std::optional<T>* target)
{
    if (value.isNull())
    {
        *target = std::nullopt;
        return;
    }
    T val;
    deserialize_field(value, &val);
    *target = val;
}


NX_FUSION_API void serialize_field(const std::vector<nx::Uuid>&value, QVariant *target);
NX_FUSION_API void deserialize_field(const QVariant &value, std::vector<nx::Uuid> *target);

#endif // QN_SERIALIZATION_SQL_FUNCTIONS_H
