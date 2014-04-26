#ifndef QN_SERIALIZATION_SQL_H
#define QN_SERIALIZATION_SQL_H

#include <cassert>

#include <type_traits> /* For std::is_same. */

#include <QtSql/QSqlRecord>
#include <QtSql/QSqlQuery>

#include <utils/common/adl_wrapper.h>

#include "sql_fwd.h"
#include "sql_index_mapping.h"

namespace QnSqlDetail {
    template<class T>
    void bind_internal(const T &value, QSqlQuery *target) {
        bind(value, target);
    }

    template<class T>
    void bind_ordered_internal(const T &value, int startIndex, QSqlQuery *target) {
        bind(value, startIndex, target);
    }

    template<class T>
    void bind_field_internal(const T &value, const QString &name, QSqlQuery *target) {
        bind_field(value, name, target);
    }

    template<class T>
    QnSqlIndexMapping mapping_internal(const QSqlQuery &query, const T *dummy) {
        return mapping(adl_wrap(query), dummy);
    }

    template<class T>
    void fetch_internal(const QnSqlIndexMapping &mapping, const QSqlRecord &value, T *target) {
        fetch(mapping, adl_wrap(value), target);
    }

    template<class T>
    void fetch_field_internal(const QSqlRecord &value, int index, T *target) {
        fetch_field(adl_wrap(value), index, target);
    }

} // namespace QnSqlDetail


namespace QnSql {
    template<class T>
    void bind(const T &value, QSqlQuery *target) {
        assert(target);

        QnSqlDetail::bind_internal(value, target);
    }

    template<class T>
    void bind_ordered(const T &value, int startIndex, QSqlQuery *target) {
        assert(target);

        QnSqlDetail::bind_ordered_internal(value, startIndex, target);
    }

    template<class T>
    void bind_field(const T &value, const QString &name, QSqlQuery *target) {
        assert(target);

        QnSqlDetail::bind_field_internal(value, name, target);
    }

    template<class T>
    QnSqlIndexMapping mapping(const QSqlQuery &query, const T *dummy = NULL) {
        return QnSqlDetail::mapping_internal(query, dummy);
    }

    template<class T>
    void fetch(const QnSqlIndexMapping &mapping, const QSqlRecord &value, T *target) {
        assert(target);

        QnSqlDetail::fetch_internal(mapping, value, target);
    }

    template<class T>
    void fetch_field(const QSqlRecord &value, int index, T *target) {
        assert(target);

        QnSqlDetail::fetch_field_internal(value, index, target);
    }

} // namespace QnSql


namespace QnSqlDetail {
    class BindVisitor {
    public:
        BindVisitor(QSqlQuery *target): 
            m_target(target) 
        {}

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) const {
            using namespace QnFusion;

            QnSql::bind_field(invoke(access(getter), value), access(sql_placeholder_name), m_target);
            return true;
        }

    private:
        QSqlQuery *m_target;
    };


    class BindOrderedVisitor {
    public:
        BindOrderedVisitor(int startIndex, QSqlQuery *target): 
            m_startIndex(startIndex),
            m_target(target) 
        {}

        template<class T, class Access>
        bool operator()(const T &, const Access &) const {
            return true; // TODO: #Elric #EC2
        }

    private:
        int m_startIndex;
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

            Member member;
            QnSql::fetch_field(m_value, index, &member);
            invoke(access(setter), target, member);
            return true;
        }

        template<class T, class Access, class Member>
        bool operator()(T &target, const Access &access, int index, const QnFusion::member_setter_tag &) const {
            using namespace QnFusion;

            /* Note that right now fetching into a struct member is THE ONLY
             * use case, so introducing this overload makes sense, even though
             * we won't be fetching large data structures from fields, and thus
             * won't save much on copy constructors. */

            QnSql::fetch_field(m_value, index, &(target.*access(setter)));
            return true;
        }

    private:
        const QnSqlIndexMapping &m_mapping;
        const QSqlRecord &m_value;
    };

} // namespace QnBinaryDetail


#define QN_FUSION_DEFINE_FUNCTIONS_sql(TYPE, ... /* PREFIX */)                  \
__VA_ARGS__ void bind(const TYPE &value, QSqlQuery *target) {                   \
    QnFusion::visit_members(value, QnSqlDetail::BindVisitor(target));           \
}                                                                               \
                                                                                \
__VA_ARGS__ void bind_ordered(const TYPE &value, int startIndex, QSqlQuery *target) { \
    QnFusion::visit_members(value, QnSqlDetail::BindOrderedVisitor(startIndex, target)); \
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


#endif // QN_SERIALIZATION_SQL_H
