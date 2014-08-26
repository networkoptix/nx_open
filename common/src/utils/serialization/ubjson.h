#ifndef QN_SERIALIZATION_UBJSON_H
#define QN_SERIALIZATION_UBJSON_H

#include "ubjson_fwd.h"
#include "ubjson_reader.h"
#include "ubjson_writer.h"
#include "serialization.h"

namespace QnUbjson {
    template<class T, class Output>
    void serialize(const T &value, QnUbjsonWriter<Output> *stream) {
        QnSerialization::serialize(value, stream);
    }

    template<class T, class Input>
    bool deserialize(QnUbjsonReader<Input> *stream, T *target) {
        return QnSerialization::deserialize(stream, target);
    }

#ifndef QN_NO_QT
    template<class T>
    QByteArray serialized(const T &value) {
        QByteArray result;
        QnUbjsonWriter<QByteArray> stream(&result);
        QnUbjson::serialize(value, &stream);
        return result;
    }

    template<class T>
    T deserialized(const QByteArray &value, const T &defaultValue = T(), bool *success = NULL) {
        T target;
        QnUbjsonReader<QByteArray> stream(&value);
        bool result = QnUbjson::deserialize(&stream, &target);
        if (success)
            *success = result;
        return result ? target : defaultValue;
    }
#endif

} // namespace QnUbjson


#endif // QN_SERIALIZATION_UBJSON_H
