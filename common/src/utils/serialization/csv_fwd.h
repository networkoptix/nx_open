#ifndef QN_SERIALIZATION_CSV_FWD_H
#define QN_SERIALIZATION_CSV_FWD_H

class QByteArray;

template<class Output>
class QnCsvStreamWriter;

namespace QnCsv {
    /**
     * Tag for types that are CSV fields. 
     */
    struct field_tag {};

    /**
     * Tag for types that are CSV records. A record is serialized as a line in a
     * CSV document.
     */
    struct record_tag {};

    /**
     * Tag for types that are CSV documents. 
     */
    struct document_tag {};

} // namespace QnCsv


#define QN_FUSION_DECLARE_FUNCTIONS_csv_field(TYPE, ... /* PREFIX */)           \
__VA_ARGS__ void serialize(const TYPE &value, QnCsvStreamWriter<QByteArray> *stream); \

#define QN_FUSION_DECLARE_FUNCTIONS_csv_record(TYPE, ... /* PREFIX */)          \
__VA_ARGS__ void serialize(const TYPE &value, QnCsvStreamWriter<QByteArray> *stream); \
__VA_ARGS__ void serialize_header(const QString &prefix, QnCsvStreamWriter<QByteArray> *stream, const TYPE *value); \


#endif // QN_SERIALIZATION_CSV_FWD_H
