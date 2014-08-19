#ifndef QN_SERIALIZATION_CSV_MACROS_H
#define QN_SERIALIZATION_CSV_MACROS_H

#include <type_traits> /* For std::is_same. */

#include <utils/fusion/fusion.h>

#include "csv.h"

namespace QnCsvDetail {

    // TODO: #Elric merge two classes?

    template<class Output>
    class SerializationVisitor {
    public:
        SerializationVisitor(QnCsvStreamWriter<Output> *stream): 
            m_stream(stream),
            m_needComma(false)
        {}

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) {
            using namespace QnFusion;

            return visit(value, access, typename access_setter_category<Access>::type());
        }

    private:
        template<class T, class Access, class Member>
        bool visit(const T &value, const Access &access, const QnFusion::typed_setter_tag<Member> &) {
            using namespace QnFusion;

            typedef typename QnCsv::type_category<Member>::type category_type;
            static_assert(!std::is_same<category_type, void>::value, "CSV serialization functions are not defined for type Member.");

            return visit(value, access, static_cast<const Member *>(NULL), category_type());
        }

        template<class T, class Access, class Tag, class Member>
        bool visit(const T &value, const Access &access, const Member *, const Tag &) {
            using namespace QnFusion;

            if(m_needComma)
                m_stream->writeComma();

            QnCsv::serialize(invoke(access(getter), value), m_stream);
            m_needComma = true; /* Assume non-empty record. */

            return true;
        }
        
        template<class T, class Access, class Member>
        bool visit(const T &, const Access &, const Member *, const QnCsv::document_tag &) {
            return true; /* Just skip it. */
        }

    private:
        QnCsvStreamWriter<Output> *m_stream;
        bool m_needComma; // TODO: #Elric we can do this statically by analyzing tag sequence at compile time. Need fusion iteration extension mechanism for that...
    };


    template<class Output>
    class HeaderVisitor {
    public:
        HeaderVisitor(const QString &prefix, QnCsvStreamWriter<Output> *stream): 
            m_prefix(prefix),
            m_stream(stream),
            m_needComma(false)
        {}

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) {
            using namespace QnFusion;

            return visit(value, access, typename access_setter_category<Access>::type());
        }

    private:
        template<class T, class Access, class Member>
        bool visit(const T &value, const Access &access, const QnFusion::typed_setter_tag<Member> &) {
            using namespace QnFusion;

            typedef typename QnCsv::type_category<Member>::type category_type;
            static_assert(!std::is_same<category_type, void>::value, "CSV serialization functions are not defined for type Member.");

            return visit(value, access, static_cast<const Member *>(NULL), category_type());
        }

        template<class T, class Access, class Member, class Tag>
        bool visit(const T &, const Access &access, const Member *, const Tag &) {
            using namespace QnFusion;

            if(m_needComma)
                m_stream->writeComma();

            if(std::is_same<Tag, QnCsv::field_tag>::value) {
                m_stream->writeField(m_prefix + access(name));
            } else {
                QnCsv::serialize_header<Member>(m_prefix + access(name) + L'.', m_stream);
            }
            m_needComma = true;

            return true;
        }

        template<class T, class Access, class Member>
        bool visit(const T &, const Access &, const Member *, const QnCsv::document_tag &) {
            return true; /* Just skip it. */
        }

    private:
        const QString &m_prefix;
        QnCsvStreamWriter<Output> *m_stream;
        bool m_needComma; // TODO: #Elric we can do this statically by analyzing tag sequence at compile time. Need fusion iteration extension mechanism for that...
    };

} // namespace QnCsvDetail


/**
 * This macro explicitly defines the csv category of the given type. This may 
 * come handy when the automatic category assignment machinery doesn't work as
 * expected, e.g. for string types which are containers and thus are
 * classified as csv documents.
 * 
 * \param TYPE                          Type to explicitly specify csv category for.
 * \param CATEGORY                      Csv category, one of <tt>QnCsv::field_tag</tt>, 
 *                                      <tt>QnCsv::record_tag</tt> and <tt>QnCsv::document_tag</tt>.
 * \param PREFIX                        Optional function definition prefix.
 */
#define QN_DECLARE_CSV_TYPE_CATEGORY(TYPE, CATEGORY, ... /* PREFIX */)          \
__VA_ARGS__ CATEGORY csv_type_category(const TYPE *);


#define QN_FUSION_DEFINE_FUNCTIONS_csv_record(TYPE, ... /* PREFIX */)           \
__VA_ARGS__ void serialize(const TYPE &value, QnCsvStreamWriter<QByteArray> *stream) { \
    QnCsvDetail::SerializationVisitor<QByteArray> visitor(stream);              \
    QnFusion::visit_members(value, visitor);                                    \
}                                                                               \
                                                                                \
__VA_ARGS__ void serialize_header(const QString &prefix, QnCsvStreamWriter<QByteArray> *stream, const TYPE *dummy) { \
    QnCsvDetail::HeaderVisitor<QByteArray> visitor(prefix, stream);             \
    QnFusion::visit_members(*dummy, visitor);                                   \
}                                                                               \


// TODO: #Elric we have undefined behaviour here^: dereferencing NULL.

#endif // QN_SERIALIZATION_CSV_MACROS_H
