#ifndef QN_SERIALIZATION_SQL_FUNCTIONS_H
#define QN_SERIALIZATION_SQL_FUNCTIONS_H

#include <chrono>
#include <string>

#include <nx/utils/uuid.h>

#include "sql.h"
#include "sql_macros.h"
#include "enum.h"

// TODO: #Elric enumz!
// TODO: #Elric #EC2 static NX_ASSERT for enum size.

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


inline void serialize_field(const QnUuid &value, QVariant *target) {
    *target = QVariant::fromValue<QByteArray>(value.toRfc4122());
}

inline void deserialize_field(const QVariant &value, QnUuid *target) {
    *target = QnUuid::fromRfc4122(value.value<QByteArray>());
}

inline void serialize_field(const std::chrono::milliseconds &value, QVariant *target) {
    *target = QVariant::fromValue<qint64>(value.count());
}

inline void deserialize_field(const QVariant &value, std::chrono::milliseconds *target) {
    *target = std::chrono::milliseconds(value.value<qint64>());
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
    QnSerialization::check_enum_binary<T>();

    QnSql::serialize_field(static_cast<qint32>(value), target);
}

template<class T>
void deserialize_field(const QVariant &value, T *target, typename std::enable_if<std::is_enum<T>::value>::type * = NULL) {
    QnSerialization::check_enum_binary<T>();

    qint32 tmp;
    QnSql::deserialize_field(value, &tmp);
    *target = static_cast<T>(tmp);
}


template<class T>
inline void serialize_field(const QFlags<T> &value, QVariant *target) {
    QnSerialization::check_enum_binary<T>();

    QnSql::serialize_field(static_cast<qint32>(value), target);
}

template<class T>
inline void deserialize_field(const QVariant &value, QFlags<T> *target) {
    QnSerialization::check_enum_binary<T>();

    qint32 tmp;
    QnSql::deserialize_field(value, &tmp);
    *target = static_cast<QFlags<T> >(tmp);
}

void serialize_field(const std::vector<QnUuid>&value, QVariant *target);
void deserialize_field(const QVariant &value, std::vector<QnUuid> *target);

#endif // QN_SERIALIZATION_SQL_FUNCTIONS_H
