#ifndef QN_SERIALIZATION_SQL_FWD_H
#define QN_SERIALIZATION_SQL_FWD_H

class QSqlRecord;
class QSqlQuery;

class QnSqlIndexMapping;

#define QN_FUSION_DECLARE_FUNCTIONS_sql(TYPE, ... /* PREFIX */)                 \
__VA_ARGS__ void bind(const TYPE &value, QSqlQuery *target);                    \
__VA_ARGS__ void bind_ordered(const TYPE &value, int startIndex, QSqlQuery *target); \
__VA_ARGS__ QnSqlIndexMapping mapping(const QSqlQuery &query, const TYPE *dummy); \
__VA_ARGS__ void fetch(const QnSqlIndexMapping &mapping, const QSqlRecord &value, TYPE *target);

#endif // QN_SERIALIZATION_SQL_FWD_H
