#pragma once

#include <nx/fusion/serialization/serialization.h>

#include "binary_fwd.h"
#include "binary_stream.h"

namespace QnBinary {
    template<class T, class Output>
    void serialize(const T &value, QnOutputBinaryStream<Output> *stream) {
        QnSerialization::serialize(value, stream);
    }

    template<class T, class Input>
    bool deserialize(QnInputBinaryStream<Input> *stream, T *target) {
        return QnSerialization::deserialize(stream, target);
    }

    template<class T>
    QByteArray serialized(const T &value) {
        QByteArray result;
        QnOutputBinaryStream<QByteArray> stream(&result);
        QnBinary::serialize(value, &stream);
        return result;
    }

    template<class T>
    T deserialized(const QByteArray &value, const T &defaultValue = T(), bool *success = NULL) {
        T target;
        QnInputBinaryStream<QByteArray> stream(&value);
        bool result = QnBinary::deserialize(&stream, &target);
        if (success)
            *success = result;
        return result ? target : defaultValue;
    }

} // namespace QnBinary
