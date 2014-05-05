#ifndef QN_SERIALIZATION_CSV_H
#define QN_SERIALIZATION_CSV_H

#include "csv_fwd.h"
#include "csv_stream.h"
#include "csv_detail.h"

namespace QnCsv {

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

} // namespace QnCsv


namespace QnCsvDetail {

    template<class Output>
    class SerializationVisitor {
    public:
        SerializationVisitor(QnOutputBinaryStream<Output> *stream): 
            m_stream(stream),
            m_needComma(false)
        {}

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) const {
            return operator()(value, access, typename QnCsv::csv_category<T, Output>::type());
        }

    private:
        template<class T, class Access>
        bool operator()(const T &value, const Access &access, const QnCsv::field_tag &) const {
            using namespace QnFusion;

            if(m_needComma)
                m_stream->writeComma();

            QnCsv::serialize_field(invoke(access(getter), value), m_stream);
            m_needComma = true;
        }

        template<class T, class Access>
        bool operator()(const T &value, const Access &access, const QnCsv::record_tag &) const {
            using namespace QnFusion;

            if(m_needComma)
                m_stream->writeComma();
            
            QnCsv::serialize_record(invoke(access(getter), value), m_stream);
            m_needComma = true; /* Assume non-empty record. */
        }
        
        template<class T, class Access>
        bool operator()(const T &value, const Access &access, const QnCsv::document_tag &) const {
            return true; /* Just skip it. */
        }

    private:
        QnOutputBinaryStream<Output> *m_stream;
        bool m_needComma; // TODO: #Elric we can do this statically by analyzing tag sequence at compile time. Need fusion iteration extension mechanism for that...
    };


    template<class Output>
    class HeaderVisitor {
    public:
        HeaderVisitor(const QString &prefix, QnOutputBinaryStream<Output> *stream): 
            m_prefix(prefix),
            m_stream(stream),
            m_needComma(false)
        {}

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) const {
            return operator()(value, access, typename QnCsv::csv_category<T, Output>::type());
        }

    private:
        template<class T, class Access>
        bool operator()(const T &value, const Access &access, const QnCsv::field_tag &) const {
            using namespace QnFusion;

            if(m_needComma)
                m_stream->writeComma();

            m_stream->writeField(m_prefix + access(name));
            m_needComma = true;
        }

        template<class T, class Access>
        bool operator()(const T &value, const Access &access, const QnCsv::record_tag &) const {
            if(m_needComma)
                m_stream->writeComma();

            QnCsv::serialize_record_header(invoke(access(getter), value), m_prefix + access(name) + L'.', m_stream);
            m_needComma = true; /* Assume non-empty record. */
        }

        template<class T, class Access>
        bool operator()(const T &value, const Access &access, const QnCsv::document_tag &) const {
            return true; /* Just skip it. */
        }

    private:
        const QString &m_prefix;
        QnOutputBinaryStream<Output> *m_stream;
        bool m_needComma; // TODO: #Elric we can do this statically by analyzing tag sequence at compile time. Need fusion iteration extension mechanism for that...
    };

} // namespace QnCsvDetail



#define QN_FUSION_DEFINE_FUNCTIONS_csv_record(TYPE, ... /* PREFIX */)           \
__VA_ARGS__ void serialize_record(const TYPE &value, QnCsvStreamWriter<QByteArray> *stream) { \
    QnFusion::visit_members(value, QnCsvDetail::SerializationVisitor<QByteArray>(stream)); \
}                                                                               \
                                                                                \
__VA_ARGS__ void serialize_record_header(const TYPE &value, const QString &header, QnCsvStreamWriter<QByteArray> *stream) { \
    QnFusion::visit_members(value, QnCsvDetail::HeaderVisitor<QByteArray>(header, stream)); \
}                                                                               \


#endif // QN_SERIALIZATION_CSV_H
