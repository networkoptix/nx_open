#ifndef __COMMON_TRANSACTION_DATA_H__
#define __COMMON_TRANSACTION_DATA_H__

#include <QtSql/QtSql>
#include "nx_ec/binary_serialization_helper.h"

namespace ec2
{

struct ApiData {
    virtual ~ApiData() {}
};

}

#define QN_DECLARE_STRUCT_SQL_BINDER() \
    inline void autoBindValues(QSqlQuery& query) const;


#define QN_DEFINE_STRUCT_SQL_BINDER(TYPE, FIELD_SEQ, ...) \
    void TYPE::autoBindValues(QSqlQuery& query)  const\
{ \
    BOOST_PP_SEQ_FOR_EACH(BIND_FIELD, ~, FIELD_SEQ) \
}


#define QN_DECLARE_STRUCT_SERIALIZATORS_BINDERS() \
    QN_DECLARE_STRUCT_SERIALIZATORS(); \
    QN_DECLARE_STRUCT_SQL_BINDER();

#define QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
    QN_DEFINE_STRUCT_SERIALIZATORS(TYPE, FIELD_SEQ); \
    QN_DEFINE_STRUCT_SQL_BINDER(TYPE, FIELD_SEQ);

#define QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS_BINDERS(TYPE, BASE_TYPE, FIELD_SEQ, ... /* PREFIX */) \
	QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(TYPE, BASE_TYPE, FIELD_SEQ); \
	QN_DEFINE_STRUCT_SQL_BINDER(TYPE, FIELD_SEQ);

//#define BIND_FIELD(R, D, FIELD) query.bindValue(QString::fromLatin1(":" #FIELD), FIELD);

template <class T>
void doAutoBind(QSqlQuery& query, const char* fieldName, const T& field) {
	query.bindValue(QString::fromLatin1(fieldName), field);
}

inline void doAutoBind(QSqlQuery& query, const char* fieldName, const QString& field) {
	query.bindValue(QString::fromLatin1(fieldName), field.isNull() ? QString(QLatin1String("")) : field);
}

template <class T>
void doAutoBind(QSqlQuery& query, const char* fieldName, const std::vector<T>& field) {
	//
}

#define TO_STRING(x) #x

#define BIND_FIELD(R, D, FIELD) doAutoBind(query, ":" TO_STRING(FIELD), FIELD);

#endif // __COMMON_TRANSACTION_DATA_H__
