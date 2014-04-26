#ifndef QN_SERIALIZATION_SQL_FUNCTIONS_H
#define QN_SERIALIZATION_SQL_FUNCTIONS_H

#include "sql.h"

#include <QtCore/QUuid>

// TODO: #Elric enumz!
// TODO: #Elric #EC2 static assert for enum size.

namespace QnSqlDetail {
    template<class T>
    inline void bind_field_direct(const T &value, const QString &name, QSqlQuery *target) {
        target->bindValue(name, QVariant::fromValue<T>(value));
    }

    template<class T>
    inline void fetch_field_direct(const QSqlRecord &value, int index, T *target) {
        *target = value.value(index).value<T>();
    }

} // namespace QnSqlDetail

#define QN_DEFINE_DIRECT_SQL_FIELD_FUNCTIONS(TYPE)                              \
inline void bind_field(const TYPE &value, const QString &name, QSqlQuery *target) { \
    QnSqlDetail::bind_field_direct(value, name, target);                        \
}                                                                               \
                                                                                \
inline void fetch_field(const QSqlRecord &value, int index, TYPE *target) {     \
    QnSqlDetail::fetch_field_direct(value, index, target);                      \
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

inline void bind_field(const bool &value, const QString &name, QSqlQuery *target) {
    target->bindValue(name, value ? 1 : 0);
}

inline void fetch_field(const QSqlRecord &value, int index, bool *target) { 
    *target = value.value(index).toBool();
}


inline void bind_field(const unsigned char &value, const QString &name, QSqlQuery *target) {
    target->bindValue(name, static_cast<unsigned int>(value));
}

inline void fetch_field(const QSqlRecord &value, int index, unsigned char *target) { 
    *target = static_cast<unsigned char>(value.value(index).value<unsigned int>());
}


inline void bind_field(const signed char &value, const QString &name, QSqlQuery *target) {
    target->bindValue(name, static_cast<int>(value));
}

inline void fetch_field(const QSqlRecord &value, int index, signed char *target) { 
    *target = static_cast<unsigned char>(value.value(index).value<int>());
}


inline void bind_field(const QString &value, const QString &name, QSqlQuery *target) {
    target->bindValue(name, value.isNull() ? lit("") : value);
}

inline void fetch_field(const QSqlRecord &value, int index, QString *target) { 
    *target = value.value(index).toString();
}


inline void bind_field(const QUuid &value, const QString &name, QSqlQuery *target) {
    target->bindValue(name, value.toRfc4122());
}

inline void fetch_field(const QSqlRecord &value, int index, QUuid *target) { 
    *target = QUuid::fromRfc4122(value.value(index).toByteArray()); 
}


template<class T>
void bind_field(const T &value, const QString &name, QSqlQuery *target, typename std::enable_if<std::is_enum<T>::value>::type * = NULL) {
    QnSql::bind_field(static_cast<qint32>(value), name, target);
}

template<class T>
void fetch_field(const QSqlRecord &value, int index, T *target, typename std::enable_if<std::is_enum<T>::value>::type * = NULL) {
    qint32 tmp;
    QnSql::fetch_field(value, index, &tmp);
    *target = static_cast<T>(tmp);
}


template<class T>
inline void bind_field(const QFlags<T> &value, const QString &name, QSqlQuery *target) {
    QnSql::bind_field(static_cast<qint32>(value), name, target);
}

template<class T>
inline void fetch_field(const QSqlRecord &value, int index, QFlags<T> *target) {
    qint32 tmp;
    QnSql::fetch_field(value, index, &tmp);
    *target = static_cast<QFlags<T> >(tmp); 
}



#endif // QN_SERIALIZATION_SQL_FUNCTIONS_H
