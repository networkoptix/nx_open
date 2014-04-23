#ifndef QN_API_DATA_H
#define QN_API_DATA_H

#include "api_globals.h"

namespace ec2 {
    
    struct ApiData {
        virtual ~ApiData() {}
    };

    struct ApiIdData: ApiData {
        QnId id;
    };
#define ApiIdData_Fields (id)

    QN_FUSION_DECLARE_FUNCTIONS(ApiIdData, (binary))

} // namespace ec2






















#if 0
#ifndef Q_MOC_RUN

#define QN_DECLARE_STRUCT_SQL_BINDER() \
    inline void autoBindValues(QSqlQuery& query) const; \
    inline void autoBindValuesOrdered(QSqlQuery& query, int startIdx) const;


#define QN_DEFINE_STRUCT_SQL_BINDER(TYPE, FIELD_SEQ, ...) \
    void TYPE::autoBindValues(QSqlQuery& query)  const\
{ BOOST_PP_SEQ_FOR_EACH(BIND_FIELD, ~, FIELD_SEQ) } \
void TYPE::autoBindValuesOrdered(QSqlQuery& query, int startIdx)  const\
{ BOOST_PP_SEQ_FOR_EACH(BIND_FIELD_ORDERED, startIdx, FIELD_SEQ) }

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

inline void doAutoBind(QSqlQuery& query, const char* fieldName, const bool& field) {
    query.bindValue(QString::fromLatin1(fieldName), field ? 1 : 0);
}

inline void doAutoBind(QSqlQuery& query, const char* fieldName, const QnId& field) {
    query.bindValue(QString::fromLatin1(fieldName), field.toRfc4122());
}

template <class T>
void doAutoBind(QSqlQuery& , const char* , const std::vector<T>& ) {
    //
}

template <class T>
void doAutoBindOrdered(QSqlQuery& query, int idx, const T& field) {
    query.bindValue(idx, field);
}

inline void doAutoBindOrdered(QSqlQuery& query, int idx, const QString& field) {
    query.bindValue(idx, field.isNull() ? QString(QLatin1String("")) : field);
}

inline void doAutoBindOrdered(QSqlQuery& query, int idx, const bool& field) {
    query.bindValue(idx, field ? 1 : 0);
}

inline void doAutoBindOrdered(QSqlQuery& query, int idx, const QnId& field) {
    query.bindValue(idx, field.toRfc4122());
}

template <class T>
void doAutoBindOrdered(QSqlQuery& , int , const std::vector<T>& ) {
    //
}

#define BIND_FIELD(R, D, FIELD) doAutoBind(query, ":" BOOST_PP_STRINGIZE(FIELD), FIELD);
#define BIND_FIELD_ORDERED(R, idx, FIELD) doAutoBindOrdered(query, idx++, FIELD);

// ----------------- load query data to a object
#define TO_IDX_VAR(x) x ## idx

#define DECLARE_FIELD_IDX(R, D, FIELD) int TO_IDX_VAR(FIELD) = rec.indexOf(QLatin1String(BOOST_PP_STRINGIZE(FIELD)));
#define ASSIGN_FIELD(R, query, FIELD) if (TO_IDX_VAR(FIELD) >= 0) queryFieldToDataObj(query, TO_IDX_VAR(FIELD), value.FIELD);

#define QN_QUERY_TO_DATA_OBJECT_FILTERED(query, TYPE, data, FIELD_SEQ, FILTER, ...) \
{ \
    QSqlRecord rec = query.record();\
    BOOST_PP_SEQ_FOR_EACH(DECLARE_FIELD_IDX, ~, FIELD_SEQ) \
    while (query.next())\
    {\
        TYPE value;\
        BOOST_PP_SEQ_FOR_EACH(ASSIGN_FIELD, query, FIELD_SEQ) \
        if (FILTER(value)) \
            data.push_back(value);\
    }\
}

#define QN_QUERY_TO_DATA_OBJECT(query, TYPE, data, FIELD_SEQ, ...) QN_QUERY_TO_DATA_OBJECT_FILTERED(query, TYPE, data, FIELD_SEQ, [] (const TYPE&) {return true;})

#else // Q_MOC_RUN

#define QN_QUERY_TO_DATA_OBJECT(...)
#define QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS_BINDERS(...)
#define QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS(...)
#define QN_DEFINE_STRUCT_SQL_BINDER(...)
#define QN_DEFINE_STRUCT_SQL_BINDER(...)
#define QN_DECLARE_STRUCT_SQL_BINDER(...)
#define QN_QUERY_TO_DATA_OBJECT_FILTERED(...)

#endif // Q_MOC_RUN

inline void queryFieldToDataObj(QSqlQuery& query, int idx, bool& field) { field = query.value(idx).toBool(); }
inline void queryFieldToDataObj(QSqlQuery& query, int idx, qint32& field) { field = query.value(idx).toInt(); }
inline void queryFieldToDataObj(QSqlQuery& query, int idx, quint32& field) { field = query.value(idx).toUInt(); }
inline void queryFieldToDataObj(QSqlQuery& query, int idx, qint16& field) { field = (qint16) query.value(idx).toInt(); }
inline void queryFieldToDataObj(QSqlQuery& query, int idx, qint8& field) { field = (qint8) query.value(idx).toInt(); }
inline void queryFieldToDataObj(QSqlQuery& query, int idx, qint64& field) { field = query.value(idx).toLongLong(); }
inline void queryFieldToDataObj(QSqlQuery& query, int idx, QByteArray& field) { field = query.value(idx).toByteArray(); }
inline void queryFieldToDataObj(QSqlQuery& query, int idx, QString& field) { field = query.value(idx).toString(); }
inline void queryFieldToDataObj(QSqlQuery& query, int idx, float& field) { field = query.value(idx).toFloat(); }
inline void queryFieldToDataObj(QSqlQuery& query, int idx, QnId& field) { field = QnId::fromRfc4122(query.value(idx).toByteArray()); }
template <class T> void queryFieldToDataObj(QSqlQuery&, int, std::vector<T>&) { ; } // TODO: #Elric wtf?
template <class T> void queryFieldToDataObj(QSqlQuery& query, int idx, T& field, typename std::enable_if<std::is_enum<T>::value>::type* = NULL ) { field = (T) query.value(idx).toInt(); }

#endif

#endif // QN_API_DATA_H
