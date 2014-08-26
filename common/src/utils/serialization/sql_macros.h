#ifndef QN_SERIALIZATION_SQL_MACROS_H
#define QN_SERIALIZATION_SQL_MACROS_H

#include <utils/fusion/fusion.h>

#include "sql.h"

namespace QnSqlDetail {
    class BindVisitor {
    public:
        BindVisitor(QSqlQuery *target): 
            m_target(target) 
        {}

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) const {
            using namespace QnFusion;

            m_target->bindValue(access(sql_placeholder_name), QnSql::serialized_field(invoke(access(getter), value)));
            return true;
        }

    private:
        QSqlQuery *m_target;
    };


    class MappingVisitor {
    public:
        MappingVisitor(const QSqlQuery &query):
            m_record(query.record())
        {}

        template<class T, class Access>
        bool operator()(const T &, const Access &access, const QnFusion::start_tag &) {
            using namespace QnFusion;
            m_mapping.indices.resize(access(member_count));
            return true;
        }

        template<class T, class Access>
        bool operator()(const T &, const Access &access) {
            using namespace QnFusion;

            m_mapping.indices[access(member_index)] = m_record.indexOf(access(name));
            return true;
        }

        const QnSqlIndexMapping &mapping() const {
            return m_mapping;
        }

    private:
        QSqlRecord m_record;
        QnSqlIndexMapping m_mapping;
    };


    class FetchVisitor {
    public:
        FetchVisitor(const QnSqlIndexMapping &mapping, const QSqlRecord &value): 
            m_mapping(mapping),
            m_value(value)
        {}

        template<class T, class Access>
        bool operator()(const T &, const Access &access, const QnFusion::start_tag &) {
            using namespace QnFusion;

            assert(m_mapping.indices.size() >= access(member_count));
            return true;
        }

        template<class T, class Access>
        bool operator()(T &target, const Access &access) const {
            using namespace QnFusion;

            int index = m_mapping.indices[access(member_index)];
            if(index < 0)
                return true; /* Just continue. */

            return operator()(target, access, index, access(setter_tag));
        }

    private:
        template<class T, class Access, class Member>
        bool operator()(T &target, const Access &access, int index, const QnFusion::typed_function_setter_tag<Member> &) const {
            using namespace QnFusion;

            invoke(access(setter), target, QnSql::deserialized_field<Member>(m_value.value(index)));
            return true;
        }

        template<class T, class Access>
        bool operator()(T &target, const Access &access, int index, const QnFusion::member_setter_tag &) const {
            using namespace QnFusion;

            /* Note that right now fetching into a struct member is THE ONLY
             * use case, so introducing this overload makes sense, even though
             * we won't be fetching large data structures from fields, and thus
             * won't save much on copy constructors. */

            QnSql::deserialize_field(m_value.value(index), &(target.*access(setter)));
            return true;
        }

    private:
        const QnSqlIndexMapping &m_mapping;
        const QSqlRecord &m_value;
    };

} // namespace QnBinaryDetail


#define QN_FUSION_DEFINE_FUNCTIONS_sql_record(TYPE, ... /* PREFIX */)           \
__VA_ARGS__ void bind(const TYPE &value, QSqlQuery *target) {                   \
    QnFusion::visit_members(value, QnSqlDetail::BindVisitor(target));           \
}                                                                               \
                                                                                \
__VA_ARGS__ QnSqlIndexMapping mapping(const QSqlQuery &query, const TYPE *dummy) { \
    QnSqlDetail::MappingVisitor visitor(query);                                 \
    QnFusion::visit_members(*dummy, visitor);                                   \
    return visitor.mapping();                                                   \
}                                                                               \
                                                                                \
__VA_ARGS__ void fetch(const QnSqlIndexMapping &mapping, const QSqlRecord &value, TYPE *target) { \
    QnFusion::visit_members(*target, QnSqlDetail::FetchVisitor(mapping, value)); \
}

// TODO: #Elric we have undefined behaviour here (mapping^): dereferencing NULL.

#endif // QN_SERIALIZATION_SQL_MACROS_H
