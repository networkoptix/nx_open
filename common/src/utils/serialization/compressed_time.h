#ifndef QN_SERIALIZATION_COMPRESSED_TIME_H
#define QN_SERIALIZATION_COMPRESSED_TIME_H

#include "compressed_time_fwd.h"
#include "compressed_time_reader.h"
#include "compressed_time_writer.h"
#include "compressed_time_functions.h"
#include "serialization.h"

namespace QnCompressedTime {

    static const char SIGNED_FORMAT[3] = {'B', '2', 'S'};
    static const char UNSIGNED_FORMAT[3] = {'B', '2', 'U'};

    template<class T, class Output>
    void serialize(const T &value, QnCompressedTimeWriter<Output> *stream) {
        QnSerialization::serialize(value, stream);
    }

    template<class T, class Input>
    bool deserialize(QnCompressedTimeReader<Input> *stream, T *target) {
        return QnSerialization::deserialize(stream, target);
    }

#ifndef QN_NO_QT
    template<class T>
    QByteArray serialized(const T &value, bool signedFormat) {
        QByteArray result;
        QnCompressedTimeWriter<QByteArray> stream(&result, signedFormat);
        QnCompressedTime::serialize(value, &stream);
        return result;
    }

    template<class T>
    T deserialized(const QByteArray &value, const T &defaultValue = T(), bool *success = NULL) {
        T target;
        QnCompressedTimeReader<QByteArray> stream(&value);
        bool result = QnCompressedTime::deserialize(&stream, &target);
        if (success)
            *success = result;
        return result ? target : defaultValue;
    }
#endif

} // namespace QnCompressedTime


#endif // QN_SERIALIZATION_COMPRESSED_TIME_H
