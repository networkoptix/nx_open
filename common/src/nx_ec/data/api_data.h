#ifndef __API_DATA_H__
#define __API_DATA_H__

#include <QtSql/QtSql>
#include "nx_ec/binary_serialization_helper.h"

namespace ec2
{

struct ApiData {
    virtual ~ApiData() {}
};

}

/*
#define FIELD_TO_HASHMAP(R, D, FIELD) result[TO_STRING(FIELD)] = FIELD;

#define QN_DEFINE_STRUCT_TO_HASH_MAP(TYPE, FIELD_SEQ) \
	QMap<QByteArray, QString> TYPE::toHashMap() const { \
	QMap<QByteArray, QString> result; \
	BOOST_PP_SEQ_FOR_EACH(FIELD_TO_HASHMAP, ~, FIELD_SEQ) \
	return result; \
}

#define QN_DEFINE_DERIVED_STRUCT_TO_HASH_MAP(TYPE, BASE_TYPE, FIELD_SEQ) \
	QMap<QByteArray, QString> TYPE::toHashMap() const { \
	QMap<QByteArray, QString> result = BASE_TYPE::toHashMap(); \
	BOOST_PP_SEQ_FOR_EACH(FIELD_TO_HASHMAP, ~, FIELD_SEQ) \
	return result; \
}
*/

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

// --------------- fill query params from a object

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

// ----------------- load query data to a object
#define TO_IDX_VAR(x) x ## idx

#define DECLARE_FIELD_IDX(R, D, FIELD) int TO_IDX_VAR(FIELD) = rec.indexOf(QLatin1String(TO_STRING(FIELD)));
#define ASSIGN_FIELD(R, D, FIELD) queryFieldToDataObj(query, TO_IDX_VAR(FIELD), value.FIELD);

#define QN_QUERY_TO_DATA_OBJECT(TYPE, data, FIELD_SEQ, ...) \
{ \
	QSqlRecord rec = query.record();\
	BOOST_PP_SEQ_FOR_EACH(DECLARE_FIELD_IDX, ~, FIELD_SEQ) \
	int idx = 0;\
	while (query.next())\
	{\
		TYPE value;\
		BOOST_PP_SEQ_FOR_EACH(ASSIGN_FIELD, ~, FIELD_SEQ) \
		data.push_back(value);\
	}\
}

inline void queryFieldToDataObj(QSqlQuery& query, int idx, bool& field) { field = query.value(idx).toBool(); }
inline void queryFieldToDataObj(QSqlQuery& query, int idx, qint32& field) { field = query.value(idx).toInt(); }
inline void queryFieldToDataObj(QSqlQuery& query, int idx, qint64& field) { field = query.value(idx).toLongLong(); }
inline void queryFieldToDataObj(QSqlQuery& query, int idx, QByteArray& field) { field = query.value(idx).toByteArray(); }
inline void queryFieldToDataObj(QSqlQuery& query, int idx, QString& field) { field = query.value(idx).toString(); }
template <class T> void queryFieldToDataObj(QSqlQuery& query, int idx, std::vector<T>& field) { ; }
template <class T> void queryFieldToDataObj(QSqlQuery& query, int idx, T& field, typename std::enable_if<std::is_enum<T>::value>::type* = NULL ) { field = (T) query.value(idx).toInt(); }

#endif // __API_DATA_H__
