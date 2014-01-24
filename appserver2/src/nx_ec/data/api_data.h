#ifndef __COMMON_TRANSACTION_DATA_H__
#define __COMMON_TRANSACTION_DATA_H__

#include <QtSql/QtSql>
#include "serialization_helper.h"

namespace ec2
{

struct ApiData {
    virtual ~ApiData() {}
};

}

#define QN_DECLARE_STRUCT_SQL_BINDER() \
    void autoBindValues(QSqlQuery& query);


#define QN_DEFINE_STRUCT_SQL_BINDER(TYPE, FIELD_SEQ, ...) \
    void TYPE::autoBindValues(QSqlQuery& query) \
{ \
    BOOST_PP_SEQ_FOR_EACH(BIND_FIELD, ~, FIELD_SEQ) \
}


#define QN_DECLARE_STRUCT_SERIALIZATORS_BINDERS() \
    QN_DECLARE_STRUCT_SERIALIZATORS(); \
    QN_DECLARE_STRUCT_SQL_BINDER();

#define QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
    QN_DEFINE_STRUCT_SERIALIZATORS(TYPE, FIELD_SEQ); \
    QN_DEFINE_STRUCT_SQL_BINDER(TYPE, FIELD_SEQ);

#define BIND_FIELD(R, D, FIELD) query.bindValue(":FIELD", FIELD);



#endif // __COMMON_TRANSACTION_DATA_H__
