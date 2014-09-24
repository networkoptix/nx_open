#ifndef QN_SERIALIZATION_CSV_H
#define QN_SERIALIZATION_CSV_H

#include <utils/serialization/serialization.h>

#include "csv_fwd.h"
#include "csv_stream.h"
#include "csv_detail.h"

namespace QnCsv {

    template<class T, class Output>
    void serialize(const T &value, QnCsvStreamWriter<Output> *stream) {
        QnSerialization::serialize(value, stream);
    }

    template<class T, class Output>
    void serialize_header(const QString &prefix, QnCsvStreamWriter<Output> *stream, const T *dummy = NULL) {
        QnCsvDetail::serialize_header_internal(prefix, stream, dummy);
    }

    template<class T, class Output = QByteArray>
    struct type_category:
        QnCsvDetail::type_category<T, Output>
    {};

    template<class T>
    QByteArray serialized(const T &value) {
        QByteArray result;
        QnCsvStreamWriter<QByteArray> stream(&result);
        QnCsv::serialize(value, &stream);
        return result;
    }

} // namespace QnCsv


#endif // QN_SERIALIZATION_CSV_H
