#ifndef QN_SERIALIZATION_CSV_H
#define QN_SERIALIZATION_CSV_H

#include <type_traits> /* For std::is_same. */

#include "csv_fwd.h"
#include "csv_stream.h"
#include "csv_detail.h"

namespace QnCsv {

    // TODO: #Elric simplify API:
    //
    // serialize
    // serialize_header(T *)
    // serialize_document


    template<class T, class Output>
    void serialize_field(const T &value, QnCsvStreamWriter<Output> *stream) {
        QnCsvDetail::serialize_field_internal(value, stream);
    }

    template<class T, class Output>
    void serialize_record(const T &value, QnCsvStreamWriter<Output> *stream) {
        QnCsvDetail::serialize_record_internal(value, stream);
    }

    template<class T, class Output>
    void serialize_record_header(const T &value, const QString &prefix, QnCsvStreamWriter<Output> *stream) {
        QnCsvDetail::serialize_record_header_internal(value, prefix, stream);
    }

    template<class T, class Output>
    void serialize_document(const T &value, QnCsvStreamWriter<Output> *stream) {
        QnCsvDetail::serialize_document_internal(value, stream);
    }

    template<class T, class Output = QByteArray>
    struct csv_category:
        QnCsvDetail::csv_category<T, Output>
    {};

    template<class T>
    QByteArray serialized(const T &value) {
        return serialized(value, typename csv_category<T, QByteArray>::type());
    }

    template<class T>
    QByteArray serialized(const T &value, const field_tag &) {
        QByteArray result;
        QnCsvStreamWriter<QByteArray> stream(&result);
        QnCsv::serialize_field(value, &stream);
        return result;
    }

    template<class T>
    QByteArray serialized(const T &value, const record_tag &) {
        QByteArray result;
        QnCsvStreamWriter<QByteArray> stream(&result);
        QnCsv::serialize_record_header(value, QString(), &stream);
        stream.writeEndline();
        QnCsv::serialize_record(value, &stream);
        stream.writeEndline();
        return result;
    }

    template<class T>
    QByteArray serialized(const T &value, const document_tag &) {
        QByteArray result;
        QnCsvStreamWriter<QByteArray> stream(&result);
        QnCsv::serialize_document(value, &stream);
        return result;
    }

} // namespace QnCsv


namespace QnCsvDetail {

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

            return operator()(value, access, typename access_setter_category<Access>::type());
        }

    private:
        template<class T, class Access, class Member>
        bool operator()(const T &value, const Access &access, const QnFusion::typed_setter_tag<Member> &) {
            using namespace QnFusion;

            typedef typename QnCsv::csv_category<Member>::type category_type;
            static_assert(!std::is_same<category_type, void>::value, "CSV serialization functions are not defined for type Member.");

            return operator()(value, access, category_type());
        }

        template<class T, class Access>
        bool operator()(const T &value, const Access &access, const QnCsv::field_tag &) {
            using namespace QnFusion;

            if(m_needComma)
                m_stream->writeComma();

            QnCsv::serialize_field(invoke(access(getter), value), m_stream);
            m_needComma = true;

            return true;
        }

        template<class T, class Access>
        bool operator()(const T &value, const Access &access, const QnCsv::record_tag &) {
            using namespace QnFusion;

            if(m_needComma)
                m_stream->writeComma();
            
            QnCsv::serialize_record(invoke(access(getter), value), m_stream);
            m_needComma = true; /* Assume non-empty record. */

            return true;
        }
        
        template<class T, class Access>
        bool operator()(const T &, const Access &, const QnCsv::document_tag &) {
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

            return operator()(value, access, typename access_setter_category<Access>::type());
        }

    private:
        template<class T, class Access, class Member>
        bool operator()(const T &value, const Access &access, const QnFusion::typed_setter_tag<Member> &) {
            using namespace QnFusion;

            typedef typename QnCsv::csv_category<Member>::type category_type;
            static_assert(!std::is_same<category_type, void>::value, "CSV serialization functions are not defined for type Member.");

            return operator()(value, access, category_type());
        }

        template<class T, class Access>
        bool operator()(const T &, const Access &access, const QnCsv::field_tag &) {
            using namespace QnFusion;

            if(m_needComma)
                m_stream->writeComma();

            m_stream->writeField(m_prefix + access(name));
            m_needComma = true;

            return true;
        }

        template<class T, class Access>
        bool operator()(const T &value, const Access &access, const QnCsv::record_tag &) {
            using namespace QnFusion;

            if(m_needComma)
                m_stream->writeComma();

            QnCsv::serialize_record_header(invoke(access(getter), value), m_prefix + access(name) + L'.', m_stream);
            m_needComma = true; /* Assume non-empty record. */

            return true;
        }

        template<class T, class Access>
        bool operator()(const T &, const Access &, const QnCsv::document_tag &) {
            return true; /* Just skip it. */
        }

    private:
        const QString &m_prefix;
        QnCsvStreamWriter<Output> *m_stream;
        bool m_needComma; // TODO: #Elric we can do this statically by analyzing tag sequence at compile time. Need fusion iteration extension mechanism for that...
    };

} // namespace QnCsvDetail



#define QN_FUSION_DEFINE_FUNCTIONS_csv_record(TYPE, ... /* PREFIX */)           \
__VA_ARGS__ void serialize_record(const TYPE &value, QnCsvStreamWriter<QByteArray> *stream) { \
    QnCsvDetail::SerializationVisitor<QByteArray> visitor(stream);              \
    QnFusion::visit_members(value, visitor);                                    \
}                                                                               \
                                                                                \
__VA_ARGS__ void serialize_record_header(const TYPE &value, const QString &prefix, QnCsvStreamWriter<QByteArray> *stream) { \
    QnCsvDetail::HeaderVisitor<QByteArray> visitor(prefix, stream);             \
    QnFusion::visit_members(value, visitor);                                    \
}                                                                               \


#endif // QN_SERIALIZATION_CSV_H
