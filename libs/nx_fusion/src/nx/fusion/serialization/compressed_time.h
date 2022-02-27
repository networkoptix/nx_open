// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_SERIALIZATION_COMPRESSED_TIME_H
#define QN_SERIALIZATION_COMPRESSED_TIME_H

#include <nx/utils/buffer.h>

#include "compressed_time_fwd.h"
#include "compressed_time_reader.h"
#include "compressed_time_writer.h"
#include "serialization.h"

namespace QnCompressedTime {

    template<class T, class Output>
    void serialize(const T &value, QnCompressedTimeWriter<Output> *stream) {
        QnSerialization::serialize(value, stream);
    }

    template<class T, class Input>
    bool deserialize(QnCompressedTimeReader<Input> *stream, T *target) {
        return QnSerialization::deserialize(stream, target);
    }

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

    template<class T>
    T deserialized(const nx::ConstBufferRefType& value, const T& defaultValue = T(), bool* success = NULL)
    {
        return deserialized(
            QByteArray::fromRawData(value.data(), (int) value.size()),
            defaultValue,
            success);
    }

} // namespace QnCompressedTime


#endif // QN_SERIALIZATION_COMPRESSED_TIME_H
